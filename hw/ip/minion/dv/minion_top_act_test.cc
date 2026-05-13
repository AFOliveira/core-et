// Copyright (c) 2026 Ainekko
// SPDX-License-Identifier: Apache-2.0
//
// ELF-backed ACT runner for minion_top.

#include "Vminion_top_act_tb.h"
#include "sim_ctrl.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <limits>
#include <unordered_map>
#include <vector>

using DUT = Vminion_top_act_tb;

namespace {

constexpr uint64_t kDefaultResetVector = 0x8000000000ull;
constexpr uint8_t kRspAck = 0;
constexpr uint8_t kRspAckData = 1;
constexpr uint8_t kReqRead = 1;
constexpr uint8_t kReqWrite = 0;
constexpr uint8_t kSizeByte = 0;
constexpr uint8_t kSizeHWord = 1;
constexpr uint8_t kSizeWord = 2;
constexpr uint8_t kSizeDWord = 3;
constexpr uint8_t kSizeQWord = 4;
constexpr uint8_t kSizeHLine = 5;
constexpr uint8_t kSizeLine = 6;

struct Segment {
    uint64_t addr = 0;
    std::vector<uint8_t> data;
};

struct ElfImage {
    std::vector<Segment> segments;
    std::unordered_map<std::string, uint64_t> symbols;
};

struct L2Resp {
    uint8_t id = 0;
    uint8_t opcode = kRspAck;
    uint8_t size = kSizeWord;
    std::array<uint32_t, 8> data{};
    uint8_t delay = 0;
};

struct FetchResp {
    uint8_t delay = 0;
    uint64_t addr = 0;
};

uint16_t rd16(const std::vector<uint8_t>& b, size_t off) {
    return uint16_t(b.at(off)) | (uint16_t(b.at(off + 1)) << 8);
}

uint32_t rd32(const std::vector<uint8_t>& b, size_t off) {
    return uint32_t(rd16(b, off)) | (uint32_t(rd16(b, off + 2)) << 16);
}

uint64_t rd64(const std::vector<uint8_t>& b, size_t off) {
    return uint64_t(rd32(b, off)) | (uint64_t(rd32(b, off + 4)) << 32);
}

ElfImage load_elf64(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open ELF: " + path);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
    if (b.size() < 64 || b[0] != 0x7f || b[1] != 'E' || b[2] != 'L' || b[3] != 'F') {
        throw std::runtime_error("not an ELF file: " + path);
    }
    if (b[4] != 2 || b[5] != 1) throw std::runtime_error("expected ELF64 little-endian");

    ElfImage img;
    const uint64_t phoff = rd64(b, 32);
    const uint64_t shoff = rd64(b, 40);
    const uint16_t phentsize = rd16(b, 54);
    const uint16_t phnum = rd16(b, 56);
    const uint16_t shentsize = rd16(b, 58);
    const uint16_t shnum = rd16(b, 60);

    for (uint16_t i = 0; i < phnum; ++i) {
        const size_t off = phoff + size_t(i) * phentsize;
        const uint32_t type = rd32(b, off);
        if (type != 1) continue;
        const uint64_t file_off = rd64(b, off + 8);
        const uint64_t vaddr = rd64(b, off + 16);
        const uint64_t filesz = rd64(b, off + 32);
        const uint64_t memsz = rd64(b, off + 40);
        Segment seg;
        seg.addr = vaddr;
        seg.data.assign(size_t(memsz), 0);
        std::copy_n(b.begin() + file_off, size_t(filesz), seg.data.begin());
        img.segments.push_back(std::move(seg));
    }

    for (uint16_t i = 0; i < shnum; ++i) {
        const size_t off = shoff + size_t(i) * shentsize;
        const uint32_t type = rd32(b, off + 4);
        if (type != 2) continue;
        const uint64_t sec_off = rd64(b, off + 24);
        const uint64_t sec_size = rd64(b, off + 32);
        const uint32_t link = rd32(b, off + 40);
        const uint64_t entsize = rd64(b, off + 56);
        if (entsize == 0 || link >= shnum) continue;

        const size_t str_sh = shoff + size_t(link) * shentsize;
        const uint64_t str_off = rd64(b, str_sh + 24);
        const uint64_t str_size = rd64(b, str_sh + 32);

        for (uint64_t so = sec_off; so < sec_off + sec_size; so += entsize) {
            const uint32_t name_off = rd32(b, so);
            const uint64_t value = rd64(b, so + 8);
            if (name_off >= str_size) continue;
            const char* name = reinterpret_cast<const char*>(&b.at(str_off + name_off));
            if (*name != '\0') img.symbols.emplace(name, value);
        }
    }

    return img;
}

class Memory {
public:
    void load(const ElfImage& elf) {
        for (const auto& seg : elf.segments) {
            for (size_t i = 0; i < seg.data.size(); ++i) {
                mem_[seg.addr + i] = seg.data[i];
            }
        }
    }

    uint8_t read8(uint64_t addr) const {
        auto it = mem_.find(addr);
        return it == mem_.end() ? 0 : it->second;
    }

    void write8(uint64_t addr, uint8_t value) { mem_[addr] = value; }

    void write32(uint64_t addr, uint32_t value) {
        for (int i = 0; i < 4; ++i) write8(addr + i, uint8_t((value >> (8 * i)) & 0xffu));
    }

    uint32_t read32(uint64_t addr) const {
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) value |= uint32_t(read8(addr + i)) << (8 * i);
        return value;
    }

    std::array<uint32_t, 8> read_block256(uint64_t addr) const {
        std::array<uint32_t, 8> words{};
        const uint64_t base = addr & ~uint64_t(31);
        for (int i = 0; i < 8; ++i) words[i] = read32(base + uint64_t(i) * 4);
        return words;
    }

private:
    std::unordered_map<uint64_t, uint8_t> mem_;
};

uint8_t bytes_for_size(uint8_t size) {
    switch (size) {
        case kSizeByte: return 1;
        case kSizeHWord: return 2;
        case kSizeWord: return 4;
        case kSizeDWord: return 8;
        case kSizeQWord: return 16;
        case kSizeHLine: return 32;
        case kSizeLine: return 64;
        default: return 4;
    }
}

uint8_t data_byte(const std::array<uint32_t, 8>& words, uint8_t byte_index) {
    const uint32_t word = words[byte_index / 4];
    return uint8_t((word >> (8 * (byte_index % 4))) & 0xffu);
}

std::array<uint32_t, 8> read_wide256(const uint32_t* words) {
    std::array<uint32_t, 8> out{};
    for (int i = 0; i < 8; ++i) out[i] = words[i];
    return out;
}

void write_wide256(uint32_t* dst, const std::array<uint32_t, 8>& src) {
    for (int i = 0; i < 8; ++i) dst[i] = src[i];
}

void mirror_dcache_store(DUT* dut, Memory& mem) {
    if (!dut->debug_dcache_s4_valid_o || !dut->debug_dcache_s4_da_write_en_o) return;

    const uint64_t base = uint64_t(dut->debug_dcache_s4_addr_o) & ~uint64_t(31);
    const std::array<uint64_t, 4> data{
        uint64_t(dut->debug_dcache_s4_data0_o),
        uint64_t(dut->debug_dcache_s4_data1_o),
        uint64_t(dut->debug_dcache_s4_data2_o),
        uint64_t(dut->debug_dcache_s4_data3_o),
    };
    const uint8_t valid_l = uint8_t(dut->debug_dcache_s4_da_valid_l_o);
    const uint8_t valid_h = uint8_t(dut->debug_dcache_s4_da_valid_h_o);

    for (uint64_t bank = 0; bank < data.size(); ++bank) {
        const uint64_t addr = base + bank * 8;
        if ((valid_l >> bank) & 1u) mem.write32(addr, uint32_t(data[bank] & 0xffff'ffffu));
        if ((valid_h >> bank) & 1u) mem.write32(addr + 4, uint32_t(data[bank] >> 32));
    }
}

void idle_inputs(DUT* dut, uint64_t reset_vector) {
    dut->enabled_i = 0x1;
    dut->reset_vector_i = reset_vector;
    dut->icache_req_ready_i = 1;
    dut->icache_resp_valid_i = 0;
    for (int i = 0; i < 8; ++i) dut->icache_resp_data_i[i] = 0;
    dut->l2_evict_req_ready_i = 0x2;
    dut->l2_miss_req_ready_i = 0x4;
    dut->l2_resp_valid_i = 0;
    dut->l2_resp_id_i = 0;
    dut->l2_resp_opcode_i = 0;
    dut->l2_resp_size_i = 0;
    dut->l2_resp_qwen_i = 0;
    for (int i = 0; i < 8; ++i) dut->l2_resp_data_i[i] = 0;
    dut->debug_halt_i = 0;
    dut->debug_resume_i = 0;
    dut->debug_resethalt_i = 0;
    dut->debug_ackhavereset_i = 0;
    dut->te_enable_i = 0;
    dut->nsleepin_i = 1;
}

void reset_all(SimCtrl<DUT>& sim, int cycles) {
    sim.dut->rst_c_ni = 0;
    sim.dut->rst_w_ni = 0;
    sim.dut->rst_d_ni = 0;
    for (int i = 0; i < cycles; ++i) sim.tick();
    sim.dut->rst_c_ni = 1;
    sim.dut->rst_w_ni = 1;
    sim.dut->rst_d_ni = 1;
    sim.tick();
    Verilated::assertOn(true);
}

std::optional<std::string> arg_value(int argc, char** argv, const char* name) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0) return argv[i + 1];
    }
    return std::nullopt;
}

bool has_arg(int argc, char** argv, const char* name) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0) return true;
    }
    return false;
}

uint64_t parse_u64(const std::string& text) {
    return std::stoull(text, nullptr, 0);
}

std::string basename(const std::string& path) {
    const auto pos = path.find_last_of('/');
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

bool drive_icache(DUT* dut,
                  const Memory& mem,
                  std::queue<FetchResp>& responses,
                  uint64_t& last_fetch_addr,
                  uint8_t& last_fetch_thread) {
    constexpr uint8_t kIcacheLatencyCycles = 1;
    bool accepted = false;

    if (dut->icache_req_valid_o) {
        const uint64_t base = uint64_t(dut->icache_req_addr_o) & ~uint64_t(31);
        last_fetch_addr = uint64_t(dut->icache_req_addr_o);
        last_fetch_thread = uint8_t(dut->icache_req_thread_id_o);
        responses.push({kIcacheLatencyCycles, base});
        accepted = true;
    }

    if (!responses.empty()) {
        FetchResp& front = responses.front();
        if (front.delay == 0) {
            const auto data = mem.read_block256(front.addr);
            for (int i = 0; i < 8; ++i) dut->icache_resp_data_i[i] = data[i];
            dut->icache_resp_valid_i = 1;
            responses.pop();
        } else {
            front.delay--;
        }
    }

    return accepted;
}

bool enqueue_l2_responses(DUT* dut,
                          Memory& mem,
                          std::queue<L2Resp>& responses,
                          uint64_t& last_l2_addr,
                          uint8_t& last_l2_opcode,
                          uint8_t& last_l2_size) {
    constexpr uint8_t kL2LatencyCycles = 0;
    const auto response_delay = [&responses]() -> uint8_t {
        return responses.empty() ? kL2LatencyCycles : 0;
    };

    bool accepted = false;
    if (dut->l2_miss_req_valid_o != 0) {
        accepted = true;
        const uint8_t id = uint8_t(dut->l2_miss_req_id_o);
        const uint8_t size = uint8_t(dut->l2_miss_req_size_o);
        const uint64_t addr = uint64_t(dut->l2_miss_req_addr_o);
        last_l2_addr = addr;
        last_l2_opcode = uint8_t(dut->l2_miss_req_opcode_o);
        last_l2_size = size;
        if (dut->l2_miss_req_opcode_o == kReqRead) {
            if (size == kSizeLine) {
                responses.push({id, kRspAckData, size, mem.read_block256(addr), response_delay()});
                responses.push({id, kRspAckData, size, mem.read_block256(addr + 32), response_delay()});
            } else {
                responses.push({id, kRspAckData, size, mem.read_block256(addr), response_delay()});
            }
        }
    }

    if (dut->l2_evict_req_valid_o != 0) {
        accepted = true;
        const uint8_t id = uint8_t(dut->l2_evict_req_id_o);
        const uint8_t size = uint8_t(dut->l2_evict_req_size_o);
        const uint64_t addr = uint64_t(dut->l2_evict_req_addr_o);
        last_l2_addr = addr;
        last_l2_opcode = uint8_t(dut->l2_evict_req_opcode_o);
        last_l2_size = size;
        if (dut->l2_evict_req_opcode_o == kReqWrite) {
            const auto data = read_wide256(dut->l2_evict_req_data_o);
            const uint8_t offset = uint8_t(addr & 31u);
            const uint8_t nbytes = std::min<uint8_t>(bytes_for_size(size), 32 - offset);
            for (uint8_t i = 0; i < nbytes; ++i) {
                mem.write8(addr + i, data_byte(data, offset + i));
            }
        }
        responses.push({id, kRspAck, size, {}, response_delay()});
    }
    return accepted;
}

bool drive_l2_response(DUT* dut, std::queue<L2Resp>& responses, L2Resp& sent) {
    if (responses.empty() || !dut->l2_resp_ready_o) {
        dut->l2_resp_valid_i = 0;
        return false;
    }
    if (responses.front().delay != 0) {
        responses.front().delay--;
        dut->l2_resp_valid_i = 0;
        return false;
    }
    const L2Resp resp = responses.front();
    responses.pop();
    sent = resp;
    dut->l2_resp_valid_i = 1;
    dut->l2_resp_id_i = resp.id;
    dut->l2_resp_opcode_i = resp.opcode;
    dut->l2_resp_size_i = resp.size;
    dut->l2_resp_qwen_i = 0;
    write_wide256(dut->l2_resp_data_i, resp.data);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const auto elf_arg = arg_value(argc, argv, "--elf");
    if (!elf_arg) {
        std::fprintf(stderr, "usage: %s --elf TEST.elf [--max-cycles N]\n", argv[0]);
        return 2;
    }

    SimCtrl<DUT> sim(argc, argv);
    const uint64_t max_cycles = parse_u64(arg_value(argc, argv, "--max-cycles").value_or("2000000"));
    const bool trace_rf = has_arg(argc, argv, "--trace-rf");
    const bool trace_retire = has_arg(argc, argv, "--trace-retire");
    const bool trace_l2 = has_arg(argc, argv, "--trace-l2");
    const bool trace_dcache = has_arg(argc, argv, "--trace-dcache");
    const bool trace_fetch = has_arg(argc, argv, "--trace-fetch");
    const bool trace_pipe = has_arg(argc, argv, "--trace-pipe");
    sim.max_time = std::numeric_limits<uint64_t>::max();

    const ElfImage elf = load_elf64(*elf_arg);
    Memory mem;
    mem.load(elf);

    const auto pass_it = elf.symbols.find("core_et_act_pass");
    const auto fail_it = elf.symbols.find("core_et_act_fail");
    if (pass_it == elf.symbols.end() || fail_it == elf.symbols.end()) {
        std::fprintf(stderr, "ACT pass/fail symbols not found in %s\n", elf_arg->c_str());
        return 2;
    }
    const uint64_t pass_pc = pass_it->second;
    const uint64_t fail_pc = fail_it->second;
    const auto reset_it = elf.symbols.find("rvtest_init");
    const uint64_t reset_vector = reset_it == elf.symbols.end() ? kDefaultResetVector : reset_it->second;

    idle_inputs(sim.dut.get(), reset_vector);
    sim.dut->rst_c_ni = 1;
    sim.dut->rst_w_ni = 1;
    sim.dut->rst_d_ni = 1;
    reset_all(sim, 8);
    sim.dut->te_enable_i = 1;

    std::queue<L2Resp> responses;
    std::queue<FetchResp> fetch_responses;
    uint64_t retired = 0;
    uint64_t fetches = 0;
    uint64_t l2_transactions = 0;
    uint64_t last_pc = 0;
    uint64_t last_fetch_addr = 0;
    uint8_t last_fetch_thread = 0;
    uint32_t last_instr = 0;
    uint64_t last_l2_addr = 0;
    uint8_t last_l2_opcode = 0;
    uint8_t last_l2_size = 0;
    L2Resp sent_l2_resp;
    uint32_t last_dcache_sboard = 0;

    for (uint64_t cycle = 0; cycle < max_cycles; ++cycle) {
        const bool fetch_accepted = drive_icache(sim.dut.get(), mem, fetch_responses, last_fetch_addr, last_fetch_thread);
        fetches += fetch_accepted ? 1 : 0;
        if (trace_fetch && fetch_accepted) {
            std::printf("IF cycle=%lu t%u addr=0x%lx\n",
                        static_cast<unsigned long>(cycle),
                        unsigned(last_fetch_thread),
                        static_cast<unsigned long>(last_fetch_addr));
        }
        const bool l2_sent = drive_l2_response(sim.dut.get(), responses, sent_l2_resp);
        if (trace_l2 && l2_sent) {
            std::printf("L2R cycle=%lu ready=%u id=%u opcode=%u size=%u pending=%zu\n",
                        static_cast<unsigned long>(cycle),
                        unsigned(sim.dut->l2_resp_ready_o),
                        unsigned(sent_l2_resp.id),
                        unsigned(sent_l2_resp.opcode),
                        unsigned(sent_l2_resp.size),
                        responses.size());
        }
        const bool l2_accepted = enqueue_l2_responses(sim.dut.get(), mem, responses, last_l2_addr, last_l2_opcode, last_l2_size);
        l2_transactions += l2_accepted ? 1 : 0;
        if (trace_l2 && l2_accepted) {
            std::printf("L2 cycle=%lu miss_valid=0x%x evict_valid=0x%x id=%u addr=0x%lx opcode=%u size=%u pending=%zu\n",
                        static_cast<unsigned long>(cycle),
                        unsigned(sim.dut->l2_miss_req_valid_o),
                        unsigned(sim.dut->l2_evict_req_valid_o),
                        unsigned(sim.dut->l2_miss_req_id_o),
                        static_cast<unsigned long>(last_l2_addr),
                        unsigned(last_l2_opcode),
                        unsigned(last_l2_size),
                        responses.size());
        }
        sim.tick();
        mirror_dcache_store(sim.dut.get(), mem);

        if (trace_pipe) {
            const auto in_window = [](uint64_t pc) {
                return (pc >= 0x8000000280ull && pc <= 0x80000002c0ull) ||
                       (pc >= 0x8000005000ull && pc <= 0x8000005040ull) ||
                       pc < 0x1000ull;
            };
            const bool interesting =
                (sim.dut->debug_id_valid_o && in_window(uint64_t(sim.dut->debug_id_pc_o))) ||
                (sim.dut->debug_ex_valid_o && in_window(uint64_t(sim.dut->debug_ex_pc_o))) ||
                (sim.dut->debug_tag_valid_o && in_window(uint64_t(sim.dut->debug_tag_pc_o))) ||
                (sim.dut->debug_mem_valid_o && in_window(uint64_t(sim.dut->debug_mem_pc_o))) ||
                (sim.dut->debug_wb_valid_o && in_window(uint64_t(sim.dut->debug_wb_pc_o)));
            if (interesting) {
                std::printf(
                    "PIPE cycle=%lu x1=0x%016lx | ID v%u/q%u/st%u/tpc%u pc=0x%lx inst=0x%08x jalr=%u r1=x%u rs1=0x%016lx | "
                    "EX v%u pc=0x%lx inst=0x%08x jalr=%u op1=0x%016lx op2=0x%016lx alu=0x%016lx | "
                    "TAG v%u/take%u pc=0x%lx inst=0x%08x jalr=%u alu=0x%016lx wdata=0x%016lx npc=0x%lx | "
                    "MEM v%u pc=0x%lx | WB v%u pc=0x%lx\n",
                    static_cast<unsigned long>(cycle),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_rf_x1_o)),
                    unsigned(sim.dut->debug_id_valid_o),
                    unsigned(sim.dut->debug_id_valid_qual_o),
                    unsigned(sim.dut->debug_id_ctrl_stall_o),
                    unsigned(sim.dut->debug_id_take_pc_o),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_id_pc_o)),
                    uint32_t(sim.dut->debug_id_inst_o),
                    unsigned(sim.dut->debug_id_jalr_o),
                    unsigned(sim.dut->debug_id_raddr1_o),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_id_rs1_data_o)),
                    unsigned(sim.dut->debug_ex_valid_o),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_ex_pc_o)),
                    uint32_t(sim.dut->debug_ex_inst_o),
                    unsigned(sim.dut->debug_ex_jalr_o),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_ex_op1_o)),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_ex_op2_o)),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_ex_alu_out_o)),
                    unsigned(sim.dut->debug_tag_valid_o),
                    unsigned(sim.dut->debug_tag_take_pc_o),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_tag_pc_o)),
                    uint32_t(sim.dut->debug_tag_inst_o),
                    unsigned(sim.dut->debug_tag_jalr_o),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_tag_reg_wdata_o)),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_tag_int_wdata_o)),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_tag_npc_o)),
                    unsigned(sim.dut->debug_mem_valid_o),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_mem_pc_o)),
                    unsigned(sim.dut->debug_wb_valid_o),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_wb_pc_o)));
            }
            if (sim.dut->debug_id_div_req_o || sim.dut->debug_ex_div_req_o ||
                !sim.dut->debug_ex_div_ready_o || sim.dut->debug_wb_div_resp_valid_o ||
                sim.dut->debug_wb_div_resp_valid_early_o || sim.dut->debug_wb_div_wen_o ||
                sim.dut->debug_id_int_sboard_hazard_o) {
                std::printf(
                    "DIV cycle=%lu id_req=%u ex_req=%u ready=%u early=%u valid=%u resp_ready=%u wen=%u dest=x%u data=0x%016lx id_sboard=%u\n",
                    static_cast<unsigned long>(cycle),
                    unsigned(sim.dut->debug_id_div_req_o),
                    unsigned(sim.dut->debug_ex_div_req_o),
                    unsigned(sim.dut->debug_ex_div_ready_o),
                    unsigned(sim.dut->debug_wb_div_resp_valid_early_o),
                    unsigned(sim.dut->debug_wb_div_resp_valid_o),
                    unsigned(sim.dut->debug_wb_div_resp_ready_o),
                    unsigned(sim.dut->debug_wb_div_wen_o),
                    unsigned(sim.dut->debug_wb_div_resp_waddr_o),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_wb_div_resp_data_o)),
                    unsigned(sim.dut->debug_id_int_sboard_hazard_o));
            }
        }

        if (trace_dcache) {
            const uint32_t dcache_sboard = uint32_t(sim.dut->debug_dcache_sboard_valid_o);
            if (sim.dut->debug_dcache_rq_push_o || sim.dut->debug_dcache_rq_dealloc_o ||
                sim.dut->debug_dcache_replay_valid_o || dcache_sboard != last_dcache_sboard ||
                sim.dut->debug_dcache_sboard_x4_o) {
                std::printf("DC cycle=%lu push=%u dealloc=%u replay=%u entry=%u sb=0x%x sb_x4=%u\n",
                            static_cast<unsigned long>(cycle),
                            unsigned(sim.dut->debug_dcache_rq_push_o),
                            unsigned(sim.dut->debug_dcache_rq_dealloc_o),
                            unsigned(sim.dut->debug_dcache_replay_valid_o),
                            unsigned(sim.dut->debug_dcache_replay_entry_o),
                            dcache_sboard,
                            unsigned(sim.dut->debug_dcache_sboard_x4_o));
                if (sim.dut->debug_dcache_rq_push_o || sim.dut->debug_dcache_replay_valid_o || (dcache_sboard & 0x80u)) {
                    std::printf("DCD cycle=%lu push=t%u:x%u:fp%u replay=t%u:x%u:fp%u sb7=t%u:x%u:fp%u\n",
                                static_cast<unsigned long>(cycle),
                                unsigned(sim.dut->debug_dcache_push_thread_id_o),
                                unsigned(sim.dut->debug_dcache_push_waddr_o),
                                unsigned(sim.dut->debug_dcache_push_fp_o),
                                unsigned(sim.dut->debug_dcache_replay_thread_id_o),
                                unsigned(sim.dut->debug_dcache_replay_waddr_o),
                                unsigned(sim.dut->debug_dcache_replay_fp_o),
                                unsigned(sim.dut->debug_dcache_sboard7_thread_id_o),
                                unsigned(sim.dut->debug_dcache_sboard7_waddr_o),
                                unsigned(sim.dut->debug_dcache_sboard7_fp_o));
                }
                last_dcache_sboard = dcache_sboard;
            }
            if (sim.dut->debug_dcache_resp_int_valid_o || sim.dut->debug_dcache_resp_valid_o) {
                std::printf("DCR cycle=%lu int_valid=%u wb_valid=%u t%u x%u fp=%u data=0x%016lx\n",
                            static_cast<unsigned long>(cycle),
                            unsigned(sim.dut->debug_dcache_resp_int_valid_o),
                            unsigned(sim.dut->debug_dcache_resp_valid_o),
                            unsigned(sim.dut->debug_dcache_resp_thread_id_o),
                            unsigned(sim.dut->debug_dcache_resp_waddr_o),
                            unsigned(sim.dut->debug_dcache_resp_fp_o),
                            static_cast<unsigned long>(uint64_t(sim.dut->debug_dcache_resp_wdata_o)));
            }
            if (sim.dut->debug_dcache_l2_valid_int_o || sim.dut->debug_dcache_mh_mw_valid_early_o ||
                sim.dut->debug_dcache_mh_mw_valid_o || sim.dut->debug_dcache_md_write_en_o ||
                (sim.dut->debug_dcache_s2_valid_qual_o && sim.dut->debug_dcache_s2_replay_o)) {
                std::printf("DCS cycle=%lu l2int=%u l2vr=%u mhfill=%u dawr=%u mw_early=%u mw=%u mdwr=%u s2=v%u/replay%u/hit%u/miss%u/cache%u repl_way=0x%x tag=0x%x mw_way=0x%x refill_way=%u\n",
                            static_cast<unsigned long>(cycle),
                            unsigned(sim.dut->debug_dcache_l2_valid_int_o),
                            unsigned(sim.dut->debug_dcache_l2_valid_ready_o),
                            unsigned(sim.dut->debug_dcache_l2_mh_fill_o),
                            unsigned(sim.dut->debug_dcache_l2_da_write_o),
                            unsigned(sim.dut->debug_dcache_mh_mw_valid_early_o),
                            unsigned(sim.dut->debug_dcache_mh_mw_valid_o),
                            unsigned(sim.dut->debug_dcache_md_write_en_o),
                            unsigned(sim.dut->debug_dcache_s2_valid_qual_o),
                            unsigned(sim.dut->debug_dcache_s2_replay_o),
                            unsigned(sim.dut->debug_dcache_s2_hit_o),
                            unsigned(sim.dut->debug_dcache_s2_nack_miss_o),
                            unsigned(sim.dut->debug_dcache_s2_cacheable_o),
                            unsigned(sim.dut->debug_dcache_s2_replace_way_en_o),
                            unsigned(sim.dut->debug_dcache_s2_tag_match_qual_o),
                            unsigned(sim.dut->debug_dcache_mh_mw_way_en_o),
                            unsigned(sim.dut->debug_dcache_mh_refill_way_o));
            }
            if ((sim.dut->debug_dcache_s2_valid_masked_o && sim.dut->debug_dcache_s2_is_write_o) ||
                (sim.dut->debug_dcache_s3_valid_o && sim.dut->debug_dcache_s3_is_write_o) ||
                sim.dut->debug_dcache_s4_valid_o ||
                sim.dut->debug_dcache_s3_da_write_en_o ||
                sim.dut->debug_dcache_s4_da_write_en_o) {
                std::printf(
                    "DCW cycle=%lu "
                    "s2=v%u/w%u/hit%u addr=0x%lx typ=%u chunk=0x%x data0=0x%016lx | "
                    "s3=v%u/w%u dawr=%u addr=0x%lx chunk=0x%x store0=0x%016lx orig0=0x%016lx "
                    "merge=0x%016lx_%016lx_%016lx_%016lx | "
                    "s4=v%u dawr=%u addr=0x%lx chunk=0x%x val_l=0x%x val_h=0x%x "
                    "data=0x%016lx_%016lx_%016lx_%016lx\n",
                    static_cast<unsigned long>(cycle),
                    unsigned(sim.dut->debug_dcache_s2_valid_masked_o),
                    unsigned(sim.dut->debug_dcache_s2_is_write_o),
                    unsigned(sim.dut->debug_dcache_s2_hit_o),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_dcache_s2_addr_o)),
                    unsigned(sim.dut->debug_dcache_s2_typ_o),
                    unsigned(sim.dut->debug_dcache_s2_chunk_read_o),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_dcache_s2_data0_o)),
                    unsigned(sim.dut->debug_dcache_s3_valid_o),
                    unsigned(sim.dut->debug_dcache_s3_is_write_o),
                    unsigned(sim.dut->debug_dcache_s3_da_write_en_o),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_dcache_s3_addr_o)),
                    unsigned(sim.dut->debug_dcache_s3_chunk_read_o),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_dcache_s3_store_data0_o)),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_dcache_s3_orig_data0_o)),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_dcache_s3_merge3_o)),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_dcache_s3_merge2_o)),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_dcache_s3_merge1_o)),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_dcache_s3_merge0_o)),
                    unsigned(sim.dut->debug_dcache_s4_valid_o),
                    unsigned(sim.dut->debug_dcache_s4_da_write_en_o),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_dcache_s4_addr_o)),
                    unsigned(sim.dut->debug_dcache_s4_chunk_read_o),
                    unsigned(sim.dut->debug_dcache_s4_da_valid_l_o),
                    unsigned(sim.dut->debug_dcache_s4_da_valid_h_o),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_dcache_s4_data3_o)),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_dcache_s4_data2_o)),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_dcache_s4_data1_o)),
                    static_cast<unsigned long>(uint64_t(sim.dut->debug_dcache_s4_data0_o)));
            }
        }

        if (trace_rf && sim.dut->debug_rf_wen_o) {
            std::printf("RF cycle=%lu t%u x%u=0x%016lx\n",
                        static_cast<unsigned long>(cycle),
                        unsigned(sim.dut->debug_rf_thread_id_o),
                        unsigned(sim.dut->debug_rf_waddr_o),
                        static_cast<unsigned long>(uint64_t(sim.dut->debug_rf_wdata_o)));
        }

        if (!sim.dut->trace_instr_valid_o) continue;
        retired++;

        const uint64_t pc = uint64_t(sim.dut->trace_instr_addr_o);
        last_pc = pc;
        last_instr = uint32_t(sim.dut->trace_instr_bus_o);
        if (trace_retire) {
            std::printf("RET cycle=%lu pc=0x%lx inst=0x%08x\n",
                        static_cast<unsigned long>(cycle),
                        static_cast<unsigned long>(pc),
                        last_instr);
        }
        if (sim.dut->trace_exception_o) {
            std::printf("RVCP-SUMMARY: TEST FAILED - Test File \"%s\"\n", basename(*elf_arg).c_str());
            std::printf("ACT failure: exception at pc=0x%lx after %lu retired instructions\n",
                        static_cast<unsigned long>(pc),
                        static_cast<unsigned long>(retired));
            return 1;
        }
        if (pc == fail_pc) {
            std::printf("RVCP-SUMMARY: TEST FAILED - Test File \"%s\"\n", basename(*elf_arg).c_str());
            return 1;
        }
        if (pc == pass_pc) {
            std::printf("RVCP-SUMMARY: TEST PASSED - Test File \"%s\"\n", basename(*elf_arg).c_str());
            return 0;
        }
    }

    std::printf("RVCP-SUMMARY: TEST FAILED - Test File \"%s\"\n", basename(*elf_arg).c_str());
    std::printf("ACT failure: timeout after %lu cycles, retired=%lu, last_pc=0x%lx, last_instr=0x%08x, fetches=%lu, last_fetch=0x%lx, last_fetch_thread=%u, l2_transactions=%lu, last_l2_addr=0x%lx, last_l2_opcode=%u, last_l2_size=%u\n",
                static_cast<unsigned long>(max_cycles),
                static_cast<unsigned long>(retired),
                static_cast<unsigned long>(last_pc),
                last_instr,
                static_cast<unsigned long>(fetches),
                static_cast<unsigned long>(last_fetch_addr),
                unsigned(last_fetch_thread),
                static_cast<unsigned long>(l2_transactions),
                static_cast<unsigned long>(last_l2_addr),
                unsigned(last_l2_opcode),
                unsigned(last_l2_size));
    return 1;
}

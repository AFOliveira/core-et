// Copyright (c) 2026 Ainekko
// SPDX-License-Identifier: Apache-2.0
//
// Verilator architectural writeback proof for strict ISA migration runtime words.
// Operand preload uses rf_wr_*; destination x10 is written only by the internal
// RTL pipeline writeback FSM (issue_i), never by C++ writing the ALU result.

#include "Visa_migration_arch_wb_tb.h"
#include "sim_ctrl.h"

#include <cstdint>
#include <cstdio>

using DUT = Visa_migration_arch_wb_tb;

namespace {

constexpr uint32_t kInstPackb   = 0x80c5e52bU;
constexpr uint32_t kInstBitmixb = 0x80c5f52bU;
constexpr uint32_t kInstSummit  = 0x0075c52bU;

constexpr uint64_t kRs1Val = 0x000000000000aa11ULL;
constexpr uint64_t kRs2Val = 0x000000000000bb22ULL;

constexpr uint64_t kExpectPackb   = 0x0000000000002211ULL;
constexpr uint64_t kExpectBitmixb = 0x000000000000f895ULL;
constexpr uint64_t kExpectSummit  = 0x000000000000aa18ULL;

void clk_low(SimCtrl<DUT>& sim) {
    sim.dut->clk_i = 0;
    sim.dut->eval();
    sim.sim_time++;
}

void clk_high(SimCtrl<DUT>& sim) {
    sim.dut->clk_i = 1;
    sim.dut->eval();
    sim.sim_time++;
}

void rf_write_pulse(SimCtrl<DUT>& sim, uint8_t addr, uint64_t data) {
    sim.dut->rf_wr_addr_i     = addr;
    sim.dut->rf_wr_data_i     = data;
    sim.dut->rf_wr_en_early_i = 1;
    sim.dut->rf_wr_en_i       = 0;
    clk_low(sim);
    clk_high(sim);
    sim.dut->rf_wr_en_early_i = 0;
    sim.dut->rf_wr_en_i       = 1;
    clk_low(sim);
    clk_high(sim);
    sim.dut->rf_wr_en_i       = 0;
}

void rf_settle(SimCtrl<DUT>& sim) {
    sim.dut->inst_i  = 0x00000013U;
    sim.dut->issue_i = 0;
    clk_low(sim);
    clk_high(sim);
}

uint64_t read_reg(SimCtrl<DUT>& sim, uint8_t addr) {
    const uint32_t probe = (static_cast<uint32_t>(addr) << 15) | 0x00000013U;
    sim.dut->inst_i  = probe;
    sim.dut->issue_i = 0;
    sim.dut->eval();
    return sim.dut->rf_rd_data0_o;
}

void run_arch_wb_case(
    SimCtrl<DUT>& sim,
    uint32_t inst,
    uint64_t expect_x10,
    const char* name
) {
    rf_write_pulse(sim, 11, kRs1Val);
    rf_write_pulse(sim, 12, kRs2Val);
    rf_settle(sim);

    sim.dut->inst_i = inst;
    sim.dut->eval();

    char msg[320];
    std::snprintf(
        msg, sizeof(msg),
        "%s: inst=0x%08x legal=%d wxd=%d rd=%u alu_out=0x%016llx expect=0x%016llx",
        name, inst,
        sim.dut->legal_o, sim.dut->wxd_o, sim.dut->rd_addr_o,
        static_cast<unsigned long long>(sim.dut->alu_out_o),
        static_cast<unsigned long long>(expect_x10)
    );
    sim.check(sim.dut->legal_o, msg);
    sim.check(sim.dut->wxd_o, msg);
    sim.check(sim.dut->rd_addr_o == 10, msg);
    sim.check(sim.dut->alu_out_o == expect_x10, msg);

    sim.dut->issue_i = 1;
    clk_low(sim);
    clk_high(sim);
    sim.dut->issue_i = 0;

    bool saw_wb_commit = false;
    for (int i = 0; i < 6; ++i) {
        clk_low(sim);
        clk_high(sim);
        if (sim.dut->wb_commit_o) {
            saw_wb_commit = true;
            break;
        }
    }
    for (int i = 0; i < 4; ++i) {
        rf_settle(sim);
    }

    sim.check(saw_wb_commit, "pipeline writeback commit observed");

    const uint64_t x10 = read_reg(sim, 10);

    std::snprintf(
        msg, sizeof(msg),
        "%s RTL writeback: x10=0x%016llx expect=0x%016llx",
        name,
        static_cast<unsigned long long>(x10),
        static_cast<unsigned long long>(expect_x10)
    );
    sim.check(x10 == expect_x10, msg);
    if (x10 == expect_x10) {
        printf(
            "ARCH_WB_PASS %s inst=0x%08x x10=0x%llx wb_commit=1\n",
            name, inst, static_cast<unsigned long long>(x10)
        );
    }
}

}  // namespace

int main(int argc, char** argv) {
    SimCtrl<DUT> sim(argc, argv);
    sim.dut->rst_ni = 1;
    sim.dut->rf_wr_en_early_i = 0;
    sim.dut->rf_wr_en_i       = 0;
    sim.dut->issue_i          = 0;

    sim.dut->clk_i = 0;
    sim.dut->rst_ni = 0;
    clk_low(sim);
    clk_high(sim);
    sim.dut->rst_ni = 1;
    rf_settle(sim);

    printf("phase 2: ISA migration architectural writeback (decode+RF+pipeline WB)\n");

    run_arch_wb_case(sim, kInstPackb, kExpectPackb, "packb");
    run_arch_wb_case(sim, kInstBitmixb, kExpectBitmixb, "bitmixb");
    run_arch_wb_case(sim, kInstSummit, kExpectSummit, "aif.europeriscvsummit");

    return sim.finish();
}

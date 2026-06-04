// Copyright (c) 2026 Ainekko
// SPDX-License-Identifier: Apache-2.0

#include "Vintpipe_decode_tb.h"
#include "sim_ctrl.h"

#include <cstdint>
#include <cstdio>

using DUT = Vintpipe_decode_tb;

namespace {

struct Expect {
    bool legal;
    bool mcode;
    bool fp;
};

// encoding_plan.yaml rtl_ctrl + minion_pkg.sv (independent of RTL casex rows).
constexpr uint8_t kA1Rs1     = 1;  // A1Rs1
constexpr uint8_t kA2Rs2     = 2;  // A2Rs2
constexpr uint8_t kImmI      = 4;  // ImmI
constexpr uint8_t kAluPackb  = 8;  // AluPackb = 5'b01000
constexpr uint8_t kAluBitmixb = 9;  // AluBitmixb = 5'b01001

constexpr uint64_t kCtrlMask = 0x1FFFFFFFFFFFULL;

struct DecodeFields {
    uint64_t ctrl;
    bool legal;
    uint8_t alu_fn;
    uint8_t sel_alu1;
    uint8_t sel_alu2;
    uint8_t sel_imm;
    bool rxs1;
    bool rxs2;
    bool wxd;
};

DecodeFields capture(SimCtrl<DUT>& sim, uint32_t inst) {
    sim.dut->inst_i = inst;
    sim.dut->eval();
    return DecodeFields{
        static_cast<uint64_t>(sim.dut->inst_ctrl_o) & kCtrlMask,
        static_cast<bool>(sim.dut->legal_o),
        static_cast<uint8_t>(sim.dut->alu_fn_o),
        static_cast<uint8_t>(sim.dut->sel_alu1_o),
        static_cast<uint8_t>(sim.dut->sel_alu2_o),
        static_cast<uint8_t>(sim.dut->sel_imm_o),
        static_cast<bool>(sim.dut->rxs1_o),
        static_cast<bool>(sim.dut->rxs2_o),
        static_cast<bool>(sim.dut->wxd_o),
    };
}

void check_decode(SimCtrl<DUT>& sim, uint32_t inst, Expect expect, const char* name) {
    sim.dut->inst_i = inst;
    sim.dut->eval();

    char msg[256];
    std::snprintf(
        msg, sizeof(msg),
        "%s: inst=0x%08x legal=%d/%d mcode=%d/%d fp=%d/%d extra=%d",
        name, inst,
        sim.dut->legal_o, expect.legal,
        sim.dut->mcode_o, expect.mcode,
        sim.dut->fp_o, expect.fp,
        sim.dut->enable_extra_trans_o
    );

    sim.check(
        sim.dut->legal_o == expect.legal &&
        sim.dut->mcode_o == expect.mcode &&
        sim.dut->fp_o == expect.fp,
        msg
    );
}

void check_rtype_plan_fields(
    SimCtrl<DUT>& sim,
    uint32_t inst,
    uint8_t alu_fn,
    const char* name
) {
    const DecodeFields d = capture(sim, inst);

    char msg[320];
    std::snprintf(
        msg, sizeof(msg),
        "%s: inst=0x%08x legal=%d alu_fn=%u sel_alu1=%u sel_alu2=%u imm=%u "
        "rxs1=%d rxs2=%d wxd=%d",
        name, inst, d.legal, d.alu_fn, d.sel_alu1, d.sel_alu2, d.sel_imm,
        d.rxs1, d.rxs2, d.wxd
    );

    const bool ok =
        d.legal &&
        d.rxs1 && d.rxs2 && d.wxd &&
        d.sel_alu1 == kA1Rs1 &&
        d.sel_alu2 == kA2Rs2 &&
        d.alu_fn == alu_fn;

    sim.check(ok, msg);
}

void check_ctrl_equivalence(
    SimCtrl<DUT>& sim,
    uint32_t target_inst,
    uint32_t reference_inst,
    Expect expect,
    const char* name
) {
    const DecodeFields target = capture(sim, target_inst);
    const DecodeFields reference = capture(sim, reference_inst);

    check_decode(sim, target_inst, expect, name);
    check_decode(sim, reference_inst, expect, name);

    char msg[320];
    std::snprintf(
        msg, sizeof(msg),
        "%s: target=0x%08x ref=0x%08x ctrl_target=0x%012llx ctrl_ref=0x%012llx",
        name, target_inst, reference_inst,
        static_cast<unsigned long long>(target.ctrl),
        static_cast<unsigned long long>(reference.ctrl)
    );

    sim.check(target.ctrl == reference.ctrl, msg);
}

void check_old_encoding_retired(
    SimCtrl<DUT>& sim,
    uint32_t new_inst,
    uint32_t old_inst,
    const char* name
) {
    const DecodeFields target = capture(sim, new_inst);
    const DecodeFields old = capture(sim, old_inst);

    const bool retired = !old.legal || (old.ctrl != target.ctrl);

    char msg[320];
    std::snprintf(
        msg, sizeof(msg),
        "%s: new=0x%08x old=0x%08x target_ctrl=0x%012llx old_ctrl=0x%012llx "
        "old_legal=%d retired=%d",
        name, new_inst, old_inst,
        static_cast<unsigned long long>(target.ctrl),
        static_cast<unsigned long long>(old.ctrl),
        old.legal, retired
    );

    sim.check(retired, msg);
}

void check_must_differ(
    SimCtrl<DUT>& sim,
    uint32_t target_inst,
    uint32_t reference_inst,
    const char* name
) {
    const DecodeFields target = capture(sim, target_inst);
    const DecodeFields reference = capture(sim, reference_inst);

    char msg[320];
    std::snprintf(
        msg, sizeof(msg),
        "%s (negative): target=0x%08x ref=0x%08x ctrl_target=0x%012llx ctrl_ref=0x%012llx",
        name, target_inst, reference_inst,
        static_cast<unsigned long long>(target.ctrl),
        static_cast<unsigned long long>(reference.ctrl)
    );

    sim.check(target.ctrl != reference.ctrl, msg);
}

}  // namespace

int main(int argc, char** argv) {
    SimCtrl<DUT> sim(argc, argv);
    sim.reset();

    const bool extra = sim.dut->enable_extra_trans_o;
    const Expect int_alu{true, false, false};

    check_decode(sim, 0x00000013, {true, false, false}, "NOP");
    check_decode(sim, 0x5870007B, {true, false, true},  "FRCP_PS");
    check_decode(sim, 0x5830007B, {true, false, true},  "FLOG_PS");
    check_decode(sim, 0x5840007B, {true, false, true},  "FEXP_PS");
    check_decode(sim, 0x5880007B,
                 extra ? Expect{true, false, true} : Expect{true, true, false},
                 "FRSQ_PS");
    check_decode(sim, 0x5860007B,
                 extra ? Expect{true, false, true} : Expect{true, true, false},
                 "FSIN_PS");

    // ISA migration (canonical runtime words per artifacts/isa-integrate/MANIFEST.md).
    constexpr uint32_t kPackbNew = 0x80c5e52b;
    constexpr uint32_t kPackbOld = 0x8000603b;
    constexpr uint32_t kBitmixbNew = 0x80c5f52b;
    constexpr uint32_t kBitmixbOld = 0x8000703b;
    constexpr uint32_t kSummitNew = 0x02a5c52b;
    constexpr uint32_t kAddiRef = 0x02a58513;
    constexpr uint32_t kSummitNegative = 0x0000402a;

    check_rtype_plan_fields(sim, kPackbNew, kAluPackb, "packb");
    check_old_encoding_retired(sim, kPackbNew, kPackbOld, "packb.OLD");

    check_rtype_plan_fields(sim, kBitmixbNew, kAluBitmixb, "bitmixb");
    check_old_encoding_retired(sim, kBitmixbNew, kBitmixbOld, "bitmixb.OLD");

    check_ctrl_equivalence(sim, kSummitNew, kAddiRef, int_alu, "aif.europeriscvsummit");
    check_must_differ(sim, kSummitNegative, kAddiRef, "aif.europeriscvsummit.negative");

    return sim.finish();
}

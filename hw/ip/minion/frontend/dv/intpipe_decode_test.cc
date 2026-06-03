// Copyright (c) 2026 Ainekko
// SPDX-License-Identifier: Apache-2.0

#include "Vintpipe_decode_tb.h"
#include "sim_ctrl.h"

#include <cstdint>
#include <cstdio>
#include <string>

using DUT = Vintpipe_decode_tb;

namespace {

using CtrlWord = uint64_t;

struct Expect {
    bool legal;
    bool mcode;
    bool fp;
};

CtrlWord capture_ctrl(SimCtrl<DUT>& sim, uint32_t inst) {
    sim.dut->inst_i = inst;
    sim.dut->eval();
    return static_cast<CtrlWord>(sim.dut->inst_ctrl_o);
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

void check_decode_equivalence(
    SimCtrl<DUT>& sim,
    uint32_t target_inst,
    uint32_t reference_inst,
    Expect expect,
    const char* name
) {
    const CtrlWord target_ctrl = capture_ctrl(sim, target_inst);
    const CtrlWord reference_ctrl = capture_ctrl(sim, reference_inst);

    check_decode(sim, target_inst, expect, name);
    check_decode(sim, reference_inst, expect, name);

    char msg[320];
    std::snprintf(
        msg, sizeof(msg),
        "%s: target=0x%08x ref=0x%08x ctrl_target=0x%016llx ctrl_ref=0x%016llx",
        name,
        target_inst,
        reference_inst,
        static_cast<unsigned long long>(target_ctrl),
        static_cast<unsigned long long>(reference_ctrl)
    );

    sim.check(target_ctrl == reference_ctrl, msg);
}

void check_decode_must_differ(
    SimCtrl<DUT>& sim,
    uint32_t target_inst,
    uint32_t reference_inst,
    const char* name
) {
    const CtrlWord target_ctrl = capture_ctrl(sim, target_inst);
    const CtrlWord reference_ctrl = capture_ctrl(sim, reference_inst);

    char msg[320];
    std::snprintf(
        msg, sizeof(msg),
        "%s (negative): target=0x%08x ref=0x%08x must differ ctrl_target=0x%016llx ctrl_ref=0x%016llx",
        name,
        target_inst,
        reference_inst,
        static_cast<unsigned long long>(target_ctrl),
        static_cast<unsigned long long>(reference_ctrl)
    );

    sim.check(target_ctrl != reference_ctrl, msg);
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

    // AIFoundry ISA migration: target (new custom-1 encoding) ≡ reference.
    check_decode_equivalence(sim, 0x8020E1AB, 0x8020E1BB, int_alu, "packb_new_vs_old_opcode");
    check_decode_equivalence(sim, 0x8020F1AB, 0x8020F1BB, int_alu, "bitmixb_new_vs_old_opcode");
    check_decode_equivalence(
        sim, 0x0050C1AB, 0x00508193, int_alu, "aif.europeriscvsummit_vs_addi");

    // Negative controls: wrong funct3 on custom-1 major (0101011), not old OP-32.
    check_decode_must_differ(sim, 0x8020C1AB, 0x8020E1BB, "packb_bad_funct3");
    check_decode_must_differ(sim, 0x8020D1AB, 0x8020F1BB, "bitmixb_bad_funct3");
    check_decode_must_differ(sim, 0x0050D1AB, 0x00508193, "aif.europeriscvsummit_bad_funct3");

    return sim.finish();
}

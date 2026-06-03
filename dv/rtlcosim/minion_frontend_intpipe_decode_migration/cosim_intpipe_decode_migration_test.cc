// Copyright (c) 2026 Ainekko
// SPDX-License-Identifier: Apache-2.0
//
// ISA migration RTL cosim: decode(new target word) on migrated core-et RTL must
// equal decode(reference word) on original et-soc1 RTL. Same-word comparison
// (minion_frontend_intpipe_decode) is invalid after opcode migration.

#include "Vcosim_intpipe_decode_migration_tb.h"
#include "cosim_ctrl.h"

#include <cstdint>
#include <cstdio>

using DUT = Vcosim_intpipe_decode_migration_tb;

namespace {

constexpr uint64_t kCtrlMask = 0x1FFFFFFFFFFFULL;

void drive_pair(CosimCtrl<DUT>& sim, uint32_t target, uint32_t reference) {
    sim.dut->target_inst_i    = target;
    sim.dut->reference_inst_i = reference;
    sim.dut->eval();
}

void check_migration_equivalence(
    CosimCtrl<DUT>& sim,
    uint32_t target,
    uint32_t reference,
    const char* name
) {
    drive_pair(sim, target, reference);
    const uint64_t new_ctrl  = sim.dut->new_out_o & kCtrlMask;
    const uint64_t orig_ctrl = sim.dut->orig_out_o & kCtrlMask;

    char msg[256];
    std::snprintf(
        msg, sizeof(msg),
        "%s: target=0x%08x ref=0x%08x new_ctrl=0x%012llx orig_ctrl=0x%012llx",
        name, target, reference,
        static_cast<unsigned long long>(new_ctrl),
        static_cast<unsigned long long>(orig_ctrl)
    );

    sim.check(new_ctrl == orig_ctrl, msg);
    sim.compare(name, orig_ctrl, new_ctrl);
}

void check_migration_must_differ(
    CosimCtrl<DUT>& sim,
    uint32_t target,
    uint32_t reference,
    const char* name
) {
    drive_pair(sim, target, reference);
    const uint64_t new_ctrl  = sim.dut->new_out_o & kCtrlMask;
    const uint64_t orig_ctrl = sim.dut->orig_out_o & kCtrlMask;

    char msg[256];
    std::snprintf(
        msg, sizeof(msg),
        "%s (negative): target=0x%08x ref=0x%08x new_ctrl=0x%012llx orig_ctrl=0x%012llx",
        name, target, reference,
        static_cast<unsigned long long>(new_ctrl),
        static_cast<unsigned long long>(orig_ctrl)
    );

    sim.check(new_ctrl != orig_ctrl, msg);
}

}  // namespace

int main(int argc, char** argv) {
    CosimCtrl<DUT> sim(argc, argv);
    sim.dut->rst_ni = 1;

    printf("phase 1: ISA migration target-vs-reference decode cosim\n");

    check_migration_equivalence(sim, 0x8020E1AB, 0x8020E1BB, "packb_directed");
    check_migration_equivalence(sim, 0x80C5E52B, 0x80C5E53B, "packb_runtime_vector");

    check_migration_equivalence(sim, 0x8020F1AB, 0x8020F1BB, "bitmixb_directed");
    check_migration_equivalence(sim, 0x80C5F52B, 0x80C5F53B, "bitmixb_runtime_vector");

    check_migration_equivalence(
        sim, 0x0050C1AB, 0x00508193, "aif.europeriscvsummit_directed");
    check_migration_equivalence(
        sim, 0x0075C52B, 0x00758513, "aif.europeriscvsummit_runtime_vector");

    check_migration_must_differ(sim, 0x8020C1AB, 0x8020E1BB, "packb_bad_funct3");
    check_migration_must_differ(sim, 0x8020D1AB, 0x8020F1BB, "bitmixb_bad_funct3");
    check_migration_must_differ(
        sim, 0x0050D1AB, 0x00508193, "aif.europeriscvsummit_bad_funct3");

    return sim.finish();
}

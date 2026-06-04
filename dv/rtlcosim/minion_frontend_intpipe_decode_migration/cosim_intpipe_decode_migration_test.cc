// Copyright (c) 2026 Ainekko
// SPDX-License-Identifier: Apache-2.0
//
// ISA migration RTL cosim: migrated decoder (core-et) vs original decoder
// (ORIG_ROOT). NEW target words on the migrated side; baseline/reference words
// on the original side. OLD encodings must not decode as the migration target
// on the migrated decoder.

#include "Vcosim_intpipe_decode_migration_tb.h"
#include "cosim_ctrl.h"

#include <cstdint>
#include <cstdio>

using DUT = Vcosim_intpipe_decode_migration_tb;

namespace {

constexpr uint64_t kCtrlMask = 0x1FFFFFFFFFFFULL;

// minion_pkg.sv AluFn values (must match original CORE_FUNC_* mapping).
constexpr uint32_t kFnPackb   = 8;
constexpr uint32_t kFnBitmixb = 9;
constexpr uint32_t kFnAdd     = 0;

constexpr uint32_t kPackbNew      = 0x80c5e52b;
constexpr uint32_t kPackbOldRef = 0x8000603b;
constexpr uint32_t kBitmixbNew      = 0x80c5f52b;
constexpr uint32_t kBitmixbOldRef   = 0x8000703b;
constexpr uint32_t kSummitNew       = 0x02a5c52b;
constexpr uint32_t kAddiRef         = 0x02a58513;
constexpr uint32_t kSummitNegative  = 0x0000402a;

void drive_pair(CosimCtrl<DUT>& sim, uint32_t target, uint32_t reference) {
    sim.dut->target_inst_i    = target;
    sim.dut->reference_inst_i = reference;
    sim.dut->eval();
}

uint64_t new_ctrl(CosimCtrl<DUT>& sim) {
    return sim.dut->new_out_o & kCtrlMask;
}

uint64_t orig_ctrl(CosimCtrl<DUT>& sim) {
    return sim.dut->orig_out_o & kCtrlMask;
}

uint32_t alu_fn_from_ctrl(uint64_t ctrl) {
    // Packed minion_control_t: 8x1b flags, sel_alu2/1 (2+2), sel_imm (3), alu_dw (3), alu_fn (5).
    return static_cast<uint32_t>((ctrl >> 18) & 0x1F);
}

bool legal_from_ctrl(uint64_t ctrl) {
    return (ctrl & 1) != 0;
}

void check_migration_equivalence(
    CosimCtrl<DUT>& sim,
    uint32_t target,
    uint32_t reference,
    const char* name,
    uint64_t ctrl_mask = kCtrlMask
) {
    drive_pair(sim, target, reference);
    const uint64_t new_c  = new_ctrl(sim) & ctrl_mask;
    const uint64_t orig_c = orig_ctrl(sim) & ctrl_mask;

    char msg[256];
    std::snprintf(
        msg, sizeof(msg),
        "%s: target=0x%08x ref=0x%08x new_ctrl=0x%012llx orig_ctrl=0x%012llx mask=0x%012llx",
        name, target, reference,
        static_cast<unsigned long long>(new_c),
        static_cast<unsigned long long>(orig_c),
        static_cast<unsigned long long>(ctrl_mask)
    );

    sim.check(new_c == orig_c, msg);
    sim.compare(name, orig_c, new_c);
    printf("PASS %s: target=0x%08x ref=0x%08x\n", name, target, reference);
}

void check_old_retired_on_migrated(
    CosimCtrl<DUT>& sim,
    uint32_t old_word,
    uint32_t target_fn,
    const char* name
) {
    drive_pair(sim, old_word, old_word);
    const uint64_t new_c = new_ctrl(sim);
    const bool retired = !legal_from_ctrl(new_c) || alu_fn_from_ctrl(new_c) != target_fn;

    char msg[256];
    std::snprintf(
        msg, sizeof(msg),
        "%s: OLD=0x%08x migrated legal=%d alu_fn=%u (expect illegal or alu_fn!=%u)",
        name, old_word, legal_from_ctrl(new_c), alu_fn_from_ctrl(new_c), target_fn
    );
    sim.check(retired, msg);
    printf("PASS %s: OLD=0x%08x retired on migrated decoder\n", name, old_word);
}

void check_negative_on_migrated(
    CosimCtrl<DUT>& sim,
    uint32_t probe,
    uint64_t ref_ctrl,
    const char* name
) {
    drive_pair(sim, probe, probe);
    const uint64_t new_c = new_ctrl(sim);
    const bool bad = !legal_from_ctrl(new_c) || new_c != ref_ctrl;

    char msg[256];
    std::snprintf(
        msg, sizeof(msg),
        "%s: probe=0x%08x new_ctrl=0x%012llx ref=0x%012llx",
        name, probe,
        static_cast<unsigned long long>(new_c),
        static_cast<unsigned long long>(ref_ctrl)
    );
    sim.check(bad, msg);
    printf("PASS %s: negative probe=0x%08x\n", name, probe);
}

}  // namespace

int main(int argc, char** argv) {
    CosimCtrl<DUT> sim(argc, argv);
    sim.dut->rst_ni = 1;

    printf("ISA migration rtlcosim: minion_frontend_intpipe_decode_migration\n");

    check_migration_equivalence(sim, kPackbNew, kPackbOldRef, "packb");
    // Original OP-32 bitmixb sets gfx=Y; encoding_plan rtl_ctrl default is gfx=N.
    constexpr uint64_t kBitmixbEquivMask = kCtrlMask & ~0x2ULL;
    check_migration_equivalence(sim, kBitmixbNew, kBitmixbOldRef, "bitmixb",
                                kBitmixbEquivMask);
    check_migration_equivalence(sim, kSummitNew, kAddiRef, "aif.europeriscvsummit");

    check_old_retired_on_migrated(sim, kPackbOldRef, kFnPackb, "packb.OLD");
    check_old_retired_on_migrated(sim, kBitmixbOldRef, kFnBitmixb, "bitmixb.OLD");

    drive_pair(sim, kSummitNew, kAddiRef);
    const uint64_t summit_ref_ctrl = orig_ctrl(sim);
    check_negative_on_migrated(sim, kSummitNegative, summit_ref_ctrl,
                               "aif.europeriscvsummit.negative");

    printf("COSIM PASSED: packb bitmixb aif.europeriscvsummit migration proof\n");
    return sim.finish();
}

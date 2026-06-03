// Copyright (c) 2026 Ainekko
// SPDX-License-Identifier: Apache-2.0
//
// Migration cosim: new decoder sees the migrated (custom-1) word; original decoder
// sees the reference word (old opcode or semantically equivalent insn).

`include "soc.vh"

module cosim_intpipe_decode_migration_tb #(
  parameter bit EnableExtraTrans = 1'b0
) (
  input  logic        clk_i,
  input  logic        rst_ni,

  input  logic [31:0] target_inst_i,
  input  logic [31:0] reference_inst_i,

  output logic [44:0] new_out_o,
  output logic [44:0] orig_out_o
);

  import minion_pkg::*;
  import minion_frontend_pkg::*;

  minion_control_t new_ctrl;

  intpipe_decode #(
    .EnableExtraTrans(EnableExtraTrans)
  ) u_new (
    .inst_bits (target_inst_i),
    .inst_ctrl (new_ctrl)
  );

  assign new_out_o = $bits(minion_control_t)'(new_ctrl);

  minion_control orig_ctrl;

  intpipe_decode_orig u_orig (
    .inst_bits (reference_inst_i),
    .inst_ctrl (orig_ctrl)
  );

  assign orig_out_o = $bits(minion_control)'(orig_ctrl);

endmodule

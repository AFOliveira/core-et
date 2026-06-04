// Copyright (c) 2026 Ainekko
// SPDX-License-Identifier: Apache-2.0

/* verilator lint_off UNUSED */  // clk_i/rst_ni are present for the standard sim_ctrl harness; intpipe_decode is combinational.
module intpipe_decode_tb
  import minion_pkg::*;
#(
  /* verilator lint_off WIDTHTRUNC */  // -GEnableExtraTrans=1 passes a 32-bit literal
  parameter bit EnableExtraTrans = 1'b0
  /* verilator lint_on WIDTHTRUNC */
) (
  input  logic        clk_i,
  input  logic        rst_ni,
  input  logic [31:0] inst_i,
  output logic        enable_extra_trans_o,
  output logic        legal_o,
  output logic        mcode_o,
  output logic        fp_o,
  output logic [$bits(minion_control_t)-1:0] inst_ctrl_o,
  output logic [4:0]                     alu_fn_o,
  output logic [1:0]                     sel_alu1_o,
  output logic [1:0]                     sel_alu2_o,
  output logic [2:0]                     sel_imm_o,
  output logic                           alu_dw_o,
  output logic                           rxs1_o,
  output logic                           rxs2_o,
  output logic                           wxd_o,
  output logic                           gfx_o
);
/* verilator lint_on UNUSED */

  /* verilator lint_off UNUSEDSIGNAL */  // The TB intentionally exposes only the legal/mcode/fp seam for focused decoder checks.
  minion_control_t inst_ctrl;
  /* verilator lint_on UNUSEDSIGNAL */

  intpipe_decode #(
    .EnableExtraTrans(EnableExtraTrans)
  ) u_dut (
    .inst_bits(inst_i),
    .inst_ctrl(inst_ctrl)
  );

  assign enable_extra_trans_o = EnableExtraTrans;
  assign legal_o              = inst_ctrl.legal;
  assign mcode_o              = inst_ctrl.mcode;
  assign fp_o                 = inst_ctrl.fp;
  assign inst_ctrl_o          = inst_ctrl;
  assign alu_fn_o             = inst_ctrl.alu_fn;
  assign sel_alu1_o           = inst_ctrl.sel_alu1;
  assign sel_alu2_o           = inst_ctrl.sel_alu2;
  assign sel_imm_o            = inst_ctrl.sel_imm;
  assign alu_dw_o             = inst_ctrl.alu_dw;
  assign rxs1_o               = inst_ctrl.rxs1;
  assign rxs2_o               = inst_ctrl.rxs2;
  assign wxd_o                = inst_ctrl.wxd;
  assign gfx_o                = inst_ctrl.gfx;

endmodule

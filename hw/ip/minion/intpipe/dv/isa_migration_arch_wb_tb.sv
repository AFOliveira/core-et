// Copyright (c) 2026 Ainekko
// SPDX-License-Identifier: Apache-2.0
//
// Architectural writeback proof: intpipe_decode + intpipe_rf + intpipe_alu.
// Operand preload uses the testbench RF write port (rf_wr_*). Destination x10
// is written only by the internal 2-phase pipeline writeback FSM.

module isa_migration_arch_wb_tb
  import minion_pkg::*;
(
  input  logic        clk_i,
  input  logic        rst_ni,
  input  logic [31:0] inst_i,
  input  logic        issue_i,

  input  logic                           rf_wr_en_early_i,
  input  logic                           rf_wr_en_i,
  input  logic [XregAddrSize-1:0]        rf_wr_addr_i,
  input  logic [XregSize-1:0]            rf_wr_data_i,

  output logic                           legal_o,
  output logic                           wxd_o,
  output logic [4:0]                     rd_addr_o,
  output logic [XregSize-1:0]            rf_rd_data0_o,
  output logic [XregSize-1:0]            alu_out_o,
  output logic                           wb_commit_o,
  output logic [XregSize-1:0]            rf_rd_wb_o
);

  typedef enum logic [1:0] {
    WbIdle   = 2'd0,
    WbEarly  = 2'd1,
    WbCommit = 2'd2
  } wb_state_e;

  wb_state_e wb_state_q;
  wb_state_e wb_state_d;

  minion_control_t ctrl;
  logic [XregAddrSize-1:0] rs1_addr;
  logic [XregAddrSize-1:0] rs2_addr;
  logic [1:0][XregSize-1:0] rf_rd_data;
  logic [NrThreadsDefault-1:0][XregSize-1:0] wb_x31_unused;
  logic [XregSize-1:0] imm_sext;
  logic [XregSize-1:0] alu_in1;
  logic [XregSize-1:0] alu_in2;
  logic [XregSize-1:0] alu_result;
  logic [XregSize-1:0] alu_adder_unused;
  logic [XregSize-1:0] wb_result_q;
  logic [XregAddrSize-1:0] wb_rd_q;

  logic pipe_wr_en_early;
  logic pipe_wr_en;
  logic [XregAddrSize-1:0] pipe_wr_addr;
  logic [XregSize-1:0] pipe_wr_data;

  logic final_wr_en_early;
  logic final_wr_en;
  logic [XregAddrSize-1:0] final_wr_addr;
  logic [XregSize-1:0] final_wr_data;

  intpipe_decode u_decode (
    .inst_bits (inst_i),
    .inst_ctrl (ctrl)
  );

  assign legal_o   = ctrl.legal;
  assign wxd_o     = ctrl.wxd;
  assign rd_addr_o = inst_i[11:7];
  assign rs1_addr  = inst_i[19:15];
  assign rs2_addr  = inst_i[24:20];
  assign imm_sext  = {{(XregSize-12){inst_i[31]}}, inst_i[31:20]};

  assign final_wr_en_early = rf_wr_en_early_i | pipe_wr_en_early;
  assign final_wr_en       = rf_wr_en_i       | pipe_wr_en;
  assign final_wr_addr     = (rf_wr_en_early_i | rf_wr_en_i) ? rf_wr_addr_i : pipe_wr_addr;
  assign final_wr_data     = (rf_wr_en_early_i | rf_wr_en_i) ? rf_wr_data_i : pipe_wr_data;

  intpipe_rf u_rf (
    .clk_i         (clk_i),
    .rd_en         ('1),
    .rd_thread_id  (1'b0),
    .rd_addr       ({rs2_addr, rs1_addr}),
    .rd_data       (rf_rd_data),
    .wb_x31_reg    (wb_x31_unused),
    .wr_en         (final_wr_en),
    .wr_en_early   (final_wr_en_early),
    .wr_thread_id  (1'b0),
    .wr_addr       (final_wr_addr),
    .wr_data       (final_wr_data)
  );

  assign rf_rd_data0_o = rf_rd_data[0];
  assign rf_rd_wb_o    = rf_rd_data[0];

  assign alu_in1 = ctrl.rxs1 ? rf_rd_data[0] : '0;
  assign alu_in2 = (ctrl.sel_alu2 == A2Imm) ? imm_sext
                 : (ctrl.rxs2 ? rf_rd_data[1] : '0);

  intpipe_alu u_alu (
    .dw        (ctrl.alu_dw),
    .fn        (ctrl.alu_fn),
    .in1       (alu_in1),
    .in2       (alu_in2),
    .out       (alu_result),
    .adder_out (alu_adder_unused)
  );

  assign alu_out_o = (wb_state_q == WbCommit) ? wb_result_q : alu_result;

  always_comb begin
    wb_state_d         = wb_state_q;
    wb_commit_o        = 1'b0;
    pipe_wr_en_early   = 1'b0;
    pipe_wr_en         = 1'b0;
    pipe_wr_addr       = '0;
    pipe_wr_data       = '0;

    unique case (wb_state_q)
      WbIdle: begin
        if (issue_i && ctrl.legal && ctrl.wxd) begin
          wb_state_d = WbEarly;
        end
      end
      WbEarly: begin
        pipe_wr_en_early = 1'b1;
        pipe_wr_addr     = wb_rd_q;
        pipe_wr_data     = wb_result_q;
        wb_state_d       = WbCommit;
      end
      WbCommit: begin
        pipe_wr_en   = 1'b1;
        pipe_wr_addr = wb_rd_q;
        pipe_wr_data = wb_result_q;
        wb_commit_o  = 1'b1;
        wb_state_d   = WbIdle;
      end
      default: wb_state_d = WbIdle;
    endcase
  end

  always_ff @(posedge clk_i) begin
    if (!rst_ni) begin
      wb_state_q  <= WbIdle;
      wb_result_q <= '0;
      wb_rd_q     <= '0;
    end else begin
      wb_state_q <= wb_state_d;
      if (issue_i && ctrl.legal && ctrl.wxd) begin
        wb_result_q <= alu_result;
        wb_rd_q     <= inst_i[11:7];
      end
    end
  end

endmodule

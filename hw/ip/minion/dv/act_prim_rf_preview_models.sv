// Copyright (c) 2026 Ainekko
// SPDX-License-Identifier: Apache-2.0
//
// ACT-only Verilator models for write-preview RF primitives.

module prim_rf_1r1w_preview #(
  parameter int unsigned Width = 32,
  parameter int unsigned Depth = 32,
  parameter bit          PreviewClkSameAsRf = 1'b1,
  localparam int unsigned AddrW = $clog2(Depth)
) (
  input  logic              preview_clk_i,
  input  logic              rf_clk_i,
  input  logic              wr_data_en_1p_next_i,
  input  logic              wr_en_i,
  input  logic [AddrW-1:0]  wr_addr_i,
  input  logic [Width-1:0]  wr_data_i,
  input  logic [AddrW-1:0]  rd_addr_i,
  output logic [Width-1:0]  rd_data_o
);
  logic [Width-1:0] rf_q [Depth];

  always_ff @(posedge rf_clk_i) begin
    if (wr_en_i) begin
      rf_q[wr_addr_i] <= wr_data_i;
    end
  end

  assign rd_data_o = rf_q[rd_addr_i];
endmodule : prim_rf_1r1w_preview

module prim_rf_1r1w_reg_preview #(
  parameter int unsigned Width = 32,
  parameter int unsigned Depth = 32,
  parameter bit          PreviewClkSameAsRf = 1'b1,
  localparam int unsigned AddrW = (Depth > 1) ? $clog2(Depth) : 1
) (
  input  logic              preview_clk_i,
  input  logic              rf_clk_i,
  input  logic              wr_data_en_1p_next_i,
  input  logic [Width-1:0]  wr_data_i,
  input  logic [AddrW-1:0]  wr_addr_i,
  input  logic              wr_en_i,
  input  logic [AddrW-1:0]  rd_addr_i,
  input  logic              rd_en_i,
  output logic [Width-1:0]  rd_data_o
);
  logic [Width-1:0] rf_q [Depth];
  logic [AddrW-1:0] rd_addr_q;

  always_ff @(posedge rf_clk_i) begin
    if (rd_en_i) begin
      rd_addr_q <= rd_addr_i;
    end
    if (wr_en_i) begin
      rf_q[wr_addr_i] <= wr_data_i;
    end
  end

  assign rd_data_o = rf_q[rd_addr_q];
endmodule : prim_rf_1r1w_reg_preview

module prim_rf_1r1w_par_preview #(
  parameter int unsigned Width = 32,
  parameter int unsigned Depth = 8,
  parameter bit          PreviewClkSameAsRf = 1'b1,
  localparam int unsigned AddrW = (Depth > 1) ? $clog2(Depth) : 1
) (
  input  logic                    preview_clk_i,
  input  logic                    rf_clk_i,
  input  logic                    wr_data_en_1p_next_i,
  output logic [Width*Depth-1:0]  rd_data_o,
  input  logic [Width-1:0]        wr_data_i,
  input  logic [AddrW-1:0]        wr_addr_i,
  input  logic                    wr_en_i
);
  logic [Width-1:0] rf_q [Depth];

  always_ff @(posedge rf_clk_i) begin
    if (wr_en_i) begin
      rf_q[wr_addr_i] <= wr_data_i;
    end
  end

  always_comb begin
    rd_data_o = '0;
    for (int i = 0; i < Depth; i++) begin
      rd_data_o[i*Width +: Width] = rf_q[i];
    end
  end
endmodule : prim_rf_1r1w_par_preview

module prim_rf_single_1r1w_par_preview #(
  parameter int unsigned Width = 32,
  parameter bit          PreviewClkSameAsRf = 1'b1
) (
  input  logic             preview_clk_i,
  input  logic             rf_clk_i,
  input  logic             wr_data_en_1p_next_i,
  output logic [Width-1:0] rd_data_o,
  input  logic [Width-1:0] wr_data_i,
  input  logic             wr_en_i
);
  logic [Width-1:0] rf_q;

  always_ff @(posedge rf_clk_i) begin
    if (wr_en_i) begin
      rf_q <= wr_data_i;
    end
  end

  assign rd_data_o = rf_q;
endmodule : prim_rf_single_1r1w_par_preview

module prim_rf_1r1w_diff_preview #(
  parameter int unsigned RWidth     = 32,
  parameter int unsigned RAlignment = 32,
  parameter int unsigned WWidth     = 32,
  parameter int unsigned Entries    = 32,
  parameter bit          PreviewClkSameAsRf = 1'b1,
  localparam int unsigned R2WRatio  = WWidth / RWidth,
  localparam int unsigned REntries  = (Entries * WWidth) / RWidth * (RWidth / RAlignment),
  localparam int unsigned RAddrW    = $clog2(REntries),
  localparam int unsigned WAddrW    = $clog2(Entries)
) (
  input  logic                    preview_clk_i,
  input  logic                    rf_clk_i,
  input  logic [R2WRatio-1:0]     wr_data_en_1p_next_i,
  output logic [RWidth-1:0]       rd_data_o,
  input  logic [RAddrW-1:0]       rd_addr_i,
  input  logic [WWidth-1:0]       wr_data_i,
  input  logic [WAddrW-1:0]       wr_addr_i,
  input  logic [R2WRatio-1:0]     wr_en_i
);
  logic [WWidth-1:0]              rf_q [Entries];
  logic [WWidth*Entries-1:0]      rf_visible;
  logic [WWidth*Entries-1:0]      rf_full;
  logic [WWidth*Entries+RAlignment-1:0] rf_full_ext;
  logic [RWidth-1:0]              rf_read [REntries];

  always_comb begin
    for (int j = 0; j < Entries; j++) begin
      rf_full[j*WWidth +: WWidth] = rf_q[j];
    end

    rf_visible = rf_full;
    for (int entry = 0; entry < Entries; entry++) begin
      if (wr_addr_i == entry[WAddrW-1:0]) begin
        for (int lane = 0; lane < R2WRatio; lane++) begin
          if (wr_en_i[lane]) begin
            rf_visible[entry*WWidth + lane*RWidth +: RWidth] =
                wr_data_i[lane*RWidth +: RWidth];
          end
        end
      end
    end

    rf_full_ext = {rf_visible[RAlignment-1:0], rf_visible};

    for (int j = 0; j < REntries; j++) begin
      rf_read[j] = rf_full_ext[j*RAlignment +: RWidth];
    end
  end

  always_ff @(posedge rf_clk_i) begin
    for (int j = 0; j < R2WRatio; j++) begin
      if (wr_en_i[j]) begin
        rf_q[wr_addr_i][j*RWidth +: RWidth] <= wr_data_i[j*RWidth +: RWidth];
      end
    end
  end

  assign rd_data_o = rf_read[rd_addr_i];
endmodule : prim_rf_1r1w_diff_preview

module prim_rf_2r1w_preview #(
  parameter int unsigned         Width        = 32,
  parameter int unsigned         Entries      = 32,
  parameter logic [Entries-1:0] Zero         = '0,
  parameter logic [Entries-1:0] Parallel     = '0,
  parameter int unsigned         ParallelW    = 32,
  parameter int unsigned         Level2CkGate = 0,
  parameter bit                  PreviewClkSameAsRf = 1'b1,
  localparam int unsigned        AddrW        = $clog2(Entries)
) (
  input  logic                   preview_clk_i,
  input  logic                   rf_clk_i,
  input  logic [AddrW-1:0]       rd_addr_a_i,
  output logic [Width-1:0]       rd_data_a_o,
  input  logic [AddrW-1:0]       rd_addr_b_i,
  output logic [Width-1:0]       rd_data_b_o,
  output logic [ParallelW-1:0]   rd_par_o,
  input  logic                   wr_en_i,
  input  logic                   wr_data_en_1p_next_i,
  input  logic [AddrW-1:0]       wr_addr_i,
  input  logic [Width-1:0]       wr_data_i
);
  logic [Width-1:0] rf_q [Entries];
  logic [Width-1:0] parallel_q [Entries];

  function automatic int unsigned par_position(input int unsigned pos);
    par_position = 0;
    for (int unsigned j = 0; j < pos; j++) begin
      if (Parallel[j]) begin
        par_position++;
      end
    end
  endfunction

  always_ff @(posedge rf_clk_i) begin
    if (wr_en_i && !Zero[wr_addr_i]) begin
      rf_q[wr_addr_i] <= wr_data_i;
      if (Parallel[wr_addr_i]) begin
        parallel_q[wr_addr_i] <= wr_data_i;
      end
    end
  end

  assign rd_data_a_o = Zero[rd_addr_a_i] ? '0 : rf_q[rd_addr_a_i];
  assign rd_data_b_o = Zero[rd_addr_b_i] ? '0 : rf_q[rd_addr_b_i];

  always_comb begin
    rd_par_o = '0;
    for (int i = 0; i < Entries; i++) begin
      if (Parallel[i]) begin
        rd_par_o[Width*par_position(i) +: Width] = Zero[i] ? '0 : parallel_q[i];
      end
    end
  end
endmodule : prim_rf_2r1w_preview

module prim_rf_3r2w_preview #(
  parameter int unsigned Width        = 32,
  parameter int unsigned Entries      = 32,
  parameter int unsigned Level2CkGate = 0,
  parameter bit          PreviewClkSameAsRf = 1'b1,
  localparam int unsigned AddrW       = $clog2(Entries)
) (
  input  logic              preview_clk_i,
  input  logic              rf_clk_i,
  input  logic [AddrW-1:0]  rd_addr_a_i,
  output logic [Width-1:0]  rd_data_a_o,
  input  logic [AddrW-1:0]  rd_addr_b_i,
  output logic [Width-1:0]  rd_data_b_o,
  input  logic [AddrW-1:0]  rd_addr_c_i,
  output logic [Width-1:0]  rd_data_c_o,
  input  logic              wr_en_a_i,
  input  logic [AddrW-1:0]  wr_addr_a_i,
  input  logic              wr_data_a_en_1p_next_i,
  input  logic [Width-1:0]  wr_data_a_i,
  input  logic              wr_en_b_i,
  input  logic [AddrW-1:0]  wr_addr_b_i,
  input  logic              wr_data_b_en_1p_next_i,
  input  logic [Width-1:0]  wr_data_b_i
);
  logic [Width-1:0] rf_a_rd_a_q [Entries];
  logic [Width-1:0] rf_a_rd_b_q [Entries];
  logic [Width-1:0] rf_a_rd_c_q [Entries];
  logic [Width-1:0] rf_b_rd_a_q [Entries];
  logic [Width-1:0] rf_b_rd_b_q [Entries];
  logic [Width-1:0] rf_b_rd_c_q [Entries];
  logic [Entries-1:0] lvt_q;

  always_ff @(posedge rf_clk_i) begin
    if (wr_en_b_i) begin
      rf_b_rd_a_q[wr_addr_b_i] <= wr_data_b_i;
      rf_b_rd_b_q[wr_addr_b_i] <= wr_data_b_i;
      rf_b_rd_c_q[wr_addr_b_i] <= wr_data_b_i;
      lvt_q[wr_addr_b_i] <= 1'b1;
    end
    if (wr_en_a_i) begin
      rf_a_rd_a_q[wr_addr_a_i] <= wr_data_a_i;
      rf_a_rd_b_q[wr_addr_a_i] <= wr_data_a_i;
      rf_a_rd_c_q[wr_addr_a_i] <= wr_data_a_i;
      lvt_q[wr_addr_a_i] <= 1'b0;
    end
  end

  assign rd_data_a_o = lvt_q[rd_addr_a_i] ? rf_b_rd_a_q[rd_addr_a_i] : rf_a_rd_a_q[rd_addr_a_i];
  assign rd_data_b_o = lvt_q[rd_addr_b_i] ? rf_b_rd_b_q[rd_addr_b_i] : rf_a_rd_b_q[rd_addr_b_i];
  assign rd_data_c_o = lvt_q[rd_addr_c_i] ? rf_b_rd_c_q[rd_addr_c_i] : rf_a_rd_c_q[rd_addr_c_i];
endmodule : prim_rf_3r2w_preview

module prim_rf_1r1w_dec_preview #(
  parameter int unsigned Width = 32,
  parameter int unsigned Entries = 8,
  parameter bit          PreviewClkSameAsRf = 1'b1
) (
  input  logic                 preview_clk_i,
  input  logic                 rf_clk_i,
  input  logic                 wr_data_en_1p_next_i,
  input  logic [Entries-1:0]   rd_addr_i,
  output logic [Width-1:0]     rd_data_o,
  input  logic [Width-1:0]     wr_data_i,
  input  logic [Entries-1:0]   wr_addr_i,
  input  logic                 wr_en_i
);
  logic [Width-1:0] rf_q [Entries];

  always_ff @(posedge rf_clk_i) begin
    for (int i = 0; i < Entries; i++) begin
      if (wr_en_i && wr_addr_i[i]) begin
        rf_q[i] <= wr_data_i;
      end
    end
  end

  always_comb begin
    rd_data_o = '0;
    for (int i = 0; i < Entries; i++) begin
      if (rd_addr_i[i]) begin
        rd_data_o |= rf_q[i];
      end
    end
  end
endmodule : prim_rf_1r1w_dec_preview

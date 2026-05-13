// Copyright (c) 2026 Ainekko
// SPDX-License-Identifier: Apache-2.0
//
// ACT program runner wrapper for minion_top.

/* verilator lint_off SYNCASYNCNET */
module minion_top_act_tb
  import minion_pkg::*;
  import minion_dcache_pkg::*;
  import minion_frontend_pkg::*;
#(
  parameter bit DebugApbEn = 1'b0,
  parameter bit DebugMonEn = 1'b0,
  parameter bit TraceEn = 1'b1,
  parameter bit VpuEn = 1'b0
) (
  input  logic                              clk_i,
  input  logic                              rst_c_ni,
  input  logic                              rst_w_ni,
  input  logic                              rst_d_ni,
  input  logic [NrThreads-1:0]              enabled_i,
  input  logic [VaSize-1:0]                 reset_vector_i,

  input  logic                              icache_req_ready_i,
  output logic                              icache_req_valid_o,
  output logic [0:0]                        icache_req_thread_id_o,
  output logic [VaSizeExt-1:0]              icache_req_addr_o,
  input  logic                              icache_resp_valid_i,
  input  logic [FeFetchReadSize-1:0]        icache_resp_data_i,

  input  logic [DcacheL2EvictReqPorts-1:0]  l2_evict_req_ready_i,
  output logic [DcacheL2EvictReqPorts-1:0]  l2_evict_req_valid_o,
  output logic [4:0]                        l2_evict_req_id_o,
  output logic [4:0]                        l2_evict_req_opcode_o,
  output logic [PaSize-1:0]                 l2_evict_req_addr_o,
  output logic [CoreL2BlockExtSize-1:0]     l2_evict_req_data_o,
  output logic [2:0]                        l2_evict_req_size_o,
  output logic [3:0]                        l2_evict_req_qwen_o,

  input  logic [DcacheL2MissReqPorts-1:0]   l2_miss_req_ready_i,
  output logic [DcacheL2MissReqPorts-1:0]   l2_miss_req_valid_o,
  output logic [4:0]                        l2_miss_req_id_o,
  output logic [4:0]                        l2_miss_req_opcode_o,
  output logic [PaSize-1:0]                 l2_miss_req_addr_o,
  output logic [2:0]                        l2_miss_req_size_o,
  output logic [3:0]                        l2_miss_req_qwen_o,

  output logic                              l2_resp_ready_o,
  input  logic                              l2_resp_valid_i,
  input  logic [EtLinkIdSize-1:0]           l2_resp_id_i,
  input  logic [1:0]                        l2_resp_opcode_i,
  input  logic [CoreL2BlockExtSize-1:0]     l2_resp_data_i,
  input  logic [2:0]                        l2_resp_size_i,
  input  logic [3:0]                        l2_resp_qwen_i,

  input  logic [NrThreads-1:0]              debug_halt_i,
  input  logic [NrThreads-1:0]              debug_resume_i,
  input  logic [NrThreads-1:0]              debug_resethalt_i,
  input  logic [NrThreads-1:0]              debug_ackhavereset_i,
  input  logic                              te_enable_i,
  input  logic                              nsleepin_i,

  output logic                              trace_instr_valid_o,
  output logic [InstSize-1:0]               trace_instr_bus_o,
  output logic [ShireTeInstrAddrWidth-1:0]  trace_instr_addr_o,
  output logic                              trace_exception_o,

  output logic                              debug_dcache_resp_valid_o,
  output logic                              debug_dcache_resp_int_valid_o,
  output logic [XregAddrSize-1:0]           debug_dcache_resp_waddr_o,
  output logic [XregSize-1:0]               debug_dcache_resp_wdata_o,
  output logic                              debug_dcache_resp_thread_id_o,
  output logic                              debug_dcache_resp_fp_o,
  output logic [DcacheReplayqSize-1:0]      debug_dcache_sboard_valid_o,
  output logic                              debug_dcache_sboard_x4_o,
  output logic                              debug_dcache_rq_push_o,
  output logic                              debug_dcache_rq_dealloc_o,
  output logic [XregAddrSize-1:0]           debug_dcache_push_waddr_o,
  output logic                              debug_dcache_push_thread_id_o,
  output logic                              debug_dcache_push_fp_o,
  output logic                              debug_dcache_replay_valid_o,
  output logic [DcacheReplayqAddrWidth-1:0] debug_dcache_replay_entry_o,
  output logic [XregAddrSize-1:0]           debug_dcache_replay_waddr_o,
  output logic                              debug_dcache_replay_thread_id_o,
  output logic                              debug_dcache_replay_fp_o,
  output logic [XregAddrSize-1:0]           debug_dcache_sboard7_waddr_o,
  output logic                              debug_dcache_sboard7_thread_id_o,
  output logic                              debug_dcache_sboard7_fp_o,
  output logic                              debug_dcache_l2_valid_int_o,
  output logic                              debug_dcache_l2_valid_ready_o,
  output logic                              debug_dcache_l2_mh_fill_o,
  output logic                              debug_dcache_l2_da_write_o,
  output logic                              debug_dcache_mh_mw_valid_early_o,
  output logic                              debug_dcache_mh_mw_valid_o,
  output logic                              debug_dcache_md_write_en_o,
  output logic                              debug_dcache_s2_valid_qual_o,
  output logic                              debug_dcache_s2_replay_o,
  output logic                              debug_dcache_s2_hit_o,
  output logic                              debug_dcache_s2_nack_miss_o,
  output logic                              debug_dcache_s2_cacheable_o,
  output logic [DcacheWays-1:0]             debug_dcache_s2_replace_way_en_o,
  output logic [DcacheWays-1:0]             debug_dcache_s2_tag_match_qual_o,
  output logic [DcacheWays-1:0]             debug_dcache_mh_mw_way_en_o,
  output logic [DcacheWayIdxWidth-1:0]      debug_dcache_mh_refill_way_o,
  output logic                              debug_dcache_s2_valid_masked_o,
  output logic                              debug_dcache_s2_is_write_o,
  output logic [PaSize-1:0]                 debug_dcache_s2_addr_o,
  output logic [3:0]                        debug_dcache_s2_typ_o,
  output logic [DcacheLramNumBanks-1:0]     debug_dcache_s2_chunk_read_o,
  output logic [XregSize-1:0]               debug_dcache_s2_data0_o,
  output logic                              debug_dcache_s3_valid_o,
  output logic                              debug_dcache_s3_is_write_o,
  output logic [PaSize-1:0]                 debug_dcache_s3_addr_o,
  output logic [DcacheLramNumBanks-1:0]     debug_dcache_s3_chunk_read_o,
  output logic [XregSize-1:0]               debug_dcache_s3_store_data0_o,
  output logic [XregSize-1:0]               debug_dcache_s3_orig_data0_o,
  output logic [XregSize-1:0]               debug_dcache_s3_merge0_o,
  output logic [XregSize-1:0]               debug_dcache_s3_merge1_o,
  output logic [XregSize-1:0]               debug_dcache_s3_merge2_o,
  output logic [XregSize-1:0]               debug_dcache_s3_merge3_o,
  output logic                              debug_dcache_s3_da_write_en_o,
  output logic                              debug_dcache_s4_valid_o,
  output logic [PaSize-1:0]                 debug_dcache_s4_addr_o,
  output logic [DcacheLramNumBanks-1:0]     debug_dcache_s4_chunk_read_o,
  output logic [XregSize-1:0]               debug_dcache_s4_data0_o,
  output logic [XregSize-1:0]               debug_dcache_s4_data1_o,
  output logic [XregSize-1:0]               debug_dcache_s4_data2_o,
  output logic [XregSize-1:0]               debug_dcache_s4_data3_o,
  output logic                              debug_dcache_s4_da_write_en_o,
  output logic [DcacheLramNumBanks-1:0]     debug_dcache_s4_da_valid_l_o,
  output logic [DcacheLramNumBanks-1:0]     debug_dcache_s4_da_valid_h_o,

  output logic                              debug_rf_wen_o,
  output logic [XregAddrSize-1:0]           debug_rf_waddr_o,
  output logic [XregSize-1:0]               debug_rf_wdata_o,
  output logic                              debug_rf_thread_id_o,
  output logic [XregSize-1:0]               debug_rf_x1_o,

  output logic                              debug_id_valid_o,
  output logic                              debug_id_valid_qual_o,
  output logic                              debug_id_ctrl_stall_o,
  output logic                              debug_id_take_pc_o,
  output logic [PcSizeExt-1:0]              debug_id_pc_o,
  output logic [InstSize-1:0]               debug_id_inst_o,
  output logic [XregAddrSize-1:0]           debug_id_raddr1_o,
  output logic [XregSize-1:0]               debug_id_rs1_data_o,
  output logic                              debug_id_jalr_o,

  output logic                              debug_ex_valid_o,
  output logic [PcSizeExt-1:0]              debug_ex_pc_o,
  output logic [InstSize-1:0]               debug_ex_inst_o,
  output logic                              debug_ex_jalr_o,
  output logic [XregSize-1:0]               debug_ex_op1_o,
  output logic [XregSize-1:0]               debug_ex_op2_o,
  output logic [XregSize-1:0]               debug_ex_alu_out_o,

  output logic                              debug_tag_valid_o,
  output logic                              debug_tag_take_pc_o,
  output logic [PcSizeExt-1:0]              debug_tag_pc_o,
  output logic [InstSize-1:0]               debug_tag_inst_o,
  output logic                              debug_tag_jalr_o,
  output logic [XregSize-1:0]               debug_tag_reg_wdata_o,
  output logic [XregSize-1:0]               debug_tag_int_wdata_o,
  output logic [PcSizeExt-1:0]              debug_tag_npc_o,

  output logic                              debug_mem_valid_o,
  output logic [PcSizeExt-1:0]              debug_mem_pc_o,
  output logic                              debug_wb_valid_o,
  output logic [PcSizeExt-1:0]              debug_wb_pc_o,

  output logic                              debug_id_div_req_o,
  output logic                              debug_ex_div_req_o,
  output logic                              debug_ex_div_ready_o,
  output logic                              debug_wb_div_resp_valid_o,
  output logic                              debug_wb_div_resp_valid_early_o,
  output logic                              debug_wb_div_resp_ready_o,
  output logic                              debug_wb_div_wen_o,
  output logic [XregAddrSize-1:0]           debug_wb_div_resp_waddr_o,
  output logic [XregSize-1:0]               debug_wb_div_resp_data_o,
  output logic                              debug_id_int_sboard_hazard_o
);

  icache_fe_resp_t        icache_resp;
  minion_debug_in_t       debug_in;
  esr_minion_features_t   esr_features;
  et_link_minion_rsp_info_t l2_resp;

  /* verilator lint_off UNUSEDSIGNAL */
  fe_icache_req_t         icache_req;
  minion_debug_out_t      debug_out;
  trace_encoder_signals_t trace_encoder;
  neigh_sm_dbg_monitor_t  minion_dbg_signals;
  et_link_minion_evict_req_info_t l2_evict_req;
  et_link_minion_miss_req_info_t  l2_miss_req;
  /* verilator lint_on UNUSEDSIGNAL */

  always_comb begin
    icache_resp = '0;
    icache_resp.data = icache_resp_data_i;
    icache_resp.cacheable = 1'b1;

    l2_resp = '0;
    l2_resp.id = l2_resp_id_i;
    l2_resp.dest = 1'b0;
    l2_resp.wdata = (l2_resp_opcode_i == EtLinkRspAckData);
    l2_resp.opcode = et_link_rsp_opcode_e'(l2_resp_opcode_i);
    l2_resp.data = l2_resp_data_i;
    l2_resp.size = et_link_size_e'(l2_resp_size_i);
    l2_resp.qwen = l2_resp_qwen_i;

    debug_in = '0;
    debug_in.halt = debug_halt_i;
    debug_in.resume = debug_resume_i;
    debug_in.resethalt = debug_resethalt_i;
    debug_in.ackhavereset = debug_ackhavereset_i;

    esr_features = '0;
    esr_features.trap_on_ml = 1'b1;
    esr_features.trap_on_gfx = 1'b1;
    esr_features.trap_on_u_scp = 1'b1;
    esr_features.trap_on_u_cacheops = 1'b1;
  end

  assign icache_req_valid_o = icache_req_valid;
  assign icache_req_thread_id_o = icache_req.thread_id;
  assign icache_req_addr_o = icache_req.addr;

  assign l2_evict_req_id_o = l2_evict_req.id;
  assign l2_evict_req_opcode_o = l2_evict_req.opcode;
  assign l2_evict_req_addr_o = l2_evict_req.address;
  assign l2_evict_req_data_o = l2_evict_req.data;
  assign l2_evict_req_size_o = l2_evict_req.size;
  assign l2_evict_req_qwen_o = l2_evict_req.qwen;

  assign l2_miss_req_id_o = l2_miss_req.id;
  assign l2_miss_req_opcode_o = l2_miss_req.opcode;
  assign l2_miss_req_addr_o = l2_miss_req.address;
  assign l2_miss_req_size_o = l2_miss_req.size;
  assign l2_miss_req_qwen_o = l2_miss_req.qwen;

  assign trace_instr_valid_o = trace_encoder.instr_valid[0];
  assign trace_instr_bus_o = trace_encoder.instr_bus[InstSize-1:0];
  assign trace_instr_addr_o = trace_encoder.instr_addr[ShireTeInstrAddrWidth-1:0];
  assign trace_exception_o = trace_encoder.exception[0];

  assign debug_dcache_resp_valid_o = u_dut.u_core.wb_dcache_core_resp_valid;
  assign debug_dcache_resp_int_valid_o = u_dut.u_core.mem_dcache_core_resp_int_valid;
  assign debug_dcache_resp_waddr_o = u_dut.u_core.wb_dcache_core_resp.dest.addr;
  assign debug_dcache_resp_wdata_o = u_dut.u_core.wb_dcache_core_resp.data[XregSize-1:0];
  assign debug_dcache_resp_thread_id_o = u_dut.u_core.wb_dcache_core_resp.dest.thread_id;
  assign debug_dcache_resp_fp_o = u_dut.u_core.wb_dcache_core_resp.dest.fp;
  assign debug_dcache_rq_push_o = u_dut.u_core.u_dcache.s2_rq_push;
  assign debug_dcache_rq_dealloc_o = u_dut.u_core.u_dcache.s2_rq_dealloc;
  assign debug_dcache_push_waddr_o = u_dut.u_core.u_dcache.s2_req.dest.addr;
  assign debug_dcache_push_thread_id_o = u_dut.u_core.u_dcache.s2_req.dest.thread_id;
  assign debug_dcache_push_fp_o = u_dut.u_core.u_dcache.s2_req.dest.fp;
  assign debug_dcache_replay_valid_o = u_dut.u_core.u_dcache.u_replay_queue.replay_valid_o;
  assign debug_dcache_replay_entry_o = u_dut.u_core.u_dcache.u_replay_queue.replay_entry_o;
  assign debug_dcache_replay_waddr_o = u_dut.u_core.u_dcache.u_replay_queue.replay_req_o.dest.addr;
  assign debug_dcache_replay_thread_id_o = u_dut.u_core.u_dcache.u_replay_queue.replay_req_o.dest.thread_id;
  assign debug_dcache_replay_fp_o = u_dut.u_core.u_dcache.u_replay_queue.replay_req_o.dest.fp;
  assign debug_dcache_sboard7_waddr_o = u_dut.u_core.id_dcache_core_scoreboard_intpipe.dest[7].addr;
  assign debug_dcache_sboard7_thread_id_o = u_dut.u_core.id_dcache_core_scoreboard_intpipe.dest[7].thread_id;
  assign debug_dcache_sboard7_fp_o = u_dut.u_core.id_dcache_core_scoreboard_intpipe.dest[7].fp;
  assign debug_dcache_l2_valid_int_o = u_dut.u_core.u_dcache.l2_resp_valid_int;
  assign debug_dcache_l2_valid_ready_o = u_dut.u_core.u_dcache.l2_resp_valid_ready_int;
  assign debug_dcache_l2_mh_fill_o = u_dut.u_core.u_dcache.l2_resp_is_for_mh_fill;
  assign debug_dcache_l2_da_write_o = u_dut.u_core.u_dcache.l2_da_write;
  assign debug_dcache_mh_mw_valid_early_o = u_dut.u_core.u_dcache.s1_mh_mw_valid_early;
  assign debug_dcache_mh_mw_valid_o = u_dut.u_core.u_dcache.s1_mh_mw_valid;
  assign debug_dcache_md_write_en_o = u_dut.u_core.u_dcache.s1_md_write_en;
  assign debug_dcache_s2_valid_qual_o = u_dut.u_core.u_dcache.s2_valid_qual;
  assign debug_dcache_s2_replay_o = u_dut.u_core.u_dcache.s2_req.replay;
  assign debug_dcache_s2_hit_o = u_dut.u_core.u_dcache.s2_hit;
  assign debug_dcache_s2_nack_miss_o = u_dut.u_core.u_dcache.s2_nack_miss;
  assign debug_dcache_s2_cacheable_o = u_dut.u_core.u_dcache.s2_req.cacheable;
  assign debug_dcache_s2_replace_way_en_o = u_dut.u_core.u_dcache.s2_replace_way_en;
  assign debug_dcache_s2_tag_match_qual_o = u_dut.u_core.u_dcache.s2_tag_match_qual;
  assign debug_dcache_mh_mw_way_en_o = u_dut.u_core.u_dcache.s1_mh_mw_req.way_en;
  assign debug_dcache_mh_refill_way_o = u_dut.u_core.u_dcache.s4_mh_refill_way;
  assign debug_dcache_s2_valid_masked_o = u_dut.u_core.u_dcache.s2_valid_masked;
  assign debug_dcache_s2_is_write_o = u_dut.u_core.u_dcache.s2_is_write;
  assign debug_dcache_s2_addr_o = u_dut.u_core.u_dcache.s2_req.addr[PaSize-1:0];
  assign debug_dcache_s2_typ_o = u_dut.u_core.u_dcache.s2_req.typ[3:0];
  assign debug_dcache_s2_chunk_read_o = u_dut.u_core.u_dcache.s2_req.chunk_read;
  assign debug_dcache_s2_data0_o = u_dut.u_core.u_dcache.s2_req.data[0 +: XregSize];
  assign debug_dcache_s3_valid_o = u_dut.u_core.u_dcache.s3_valid;
  assign debug_dcache_s3_is_write_o = u_dut.u_core.u_dcache.s3_is_write;
  assign debug_dcache_s3_addr_o = u_dut.u_core.u_dcache.s3_req.addr[PaSize-1:0];
  assign debug_dcache_s3_chunk_read_o = u_dut.u_core.u_dcache.s3_req.chunk_read;
  assign debug_dcache_s3_store_data0_o = u_dut.u_core.u_dcache.s3_store_data[0 +: XregSize];
  assign debug_dcache_s3_orig_data0_o = u_dut.u_core.u_dcache.s3_data_bypassed[0 +: XregSize];
  assign debug_dcache_s3_merge0_o = u_dut.u_core.u_dcache.s3_store_merge_out[0*DcacheLramDataSize +: DcacheLramDataSize];
  assign debug_dcache_s3_merge1_o = u_dut.u_core.u_dcache.s3_store_merge_out[1*DcacheLramDataSize +: DcacheLramDataSize];
  assign debug_dcache_s3_merge2_o = u_dut.u_core.u_dcache.s3_store_merge_out[2*DcacheLramDataSize +: DcacheLramDataSize];
  assign debug_dcache_s3_merge3_o = u_dut.u_core.u_dcache.s3_store_merge_out[3*DcacheLramDataSize +: DcacheLramDataSize];
  assign debug_dcache_s3_da_write_en_o = u_dut.u_core.u_dcache.s3_da_write_en;
  assign debug_dcache_s4_valid_o = u_dut.u_core.u_dcache.s4_valid;
  assign debug_dcache_s4_addr_o = u_dut.u_core.u_dcache.s4_req.addr[PaSize-1:0];
  assign debug_dcache_s4_chunk_read_o = u_dut.u_core.u_dcache.s4_req.chunk_read;
  assign debug_dcache_s4_data0_o = u_dut.u_core.u_dcache.s4_req.data[0*DcacheLramDataSize +: DcacheLramDataSize];
  assign debug_dcache_s4_data1_o = u_dut.u_core.u_dcache.s4_req.data[1*DcacheLramDataSize +: DcacheLramDataSize];
  assign debug_dcache_s4_data2_o = u_dut.u_core.u_dcache.s4_req.data[2*DcacheLramDataSize +: DcacheLramDataSize];
  assign debug_dcache_s4_data3_o = u_dut.u_core.u_dcache.s4_req.data[3*DcacheLramDataSize +: DcacheLramDataSize];
  assign debug_dcache_s4_da_write_en_o = u_dut.u_core.u_dcache.s4_da_write_en_ovr;
  for (genvar dbg_bank = 0; dbg_bank < DcacheLramNumBanks; dbg_bank++) begin : gen_debug_da_valid
    assign debug_dcache_s4_da_valid_l_o[dbg_bank] = u_dut.u_core.u_dcache.s4_da_write_data[dbg_bank].valid_l;
    assign debug_dcache_s4_da_valid_h_o[dbg_bank] = u_dut.u_core.u_dcache.s4_da_write_data[dbg_bank].valid_h;
  end

  always_comb begin
    debug_dcache_sboard_valid_o = '0;
    debug_dcache_sboard_x4_o = 1'b0;
    for (int unsigned i = 0; i < DcacheReplayqSize; i++) begin
      debug_dcache_sboard_valid_o[i] = u_dut.u_core.id_dcache_core_scoreboard_intpipe.valid[i];
      debug_dcache_sboard_x4_o |=
          u_dut.u_core.id_dcache_core_scoreboard_intpipe.valid[i]
          && !u_dut.u_core.id_dcache_core_scoreboard_intpipe.dest[i].fp
          && (u_dut.u_core.id_dcache_core_scoreboard_intpipe.dest[i].thread_id == 1'b0)
          && (u_dut.u_core.id_dcache_core_scoreboard_intpipe.dest[i].addr == 5'd4);
    end
  end

  assign debug_rf_wen_o = u_dut.u_core.u_intpipe.wb_rf_wen;
  assign debug_rf_waddr_o = u_dut.u_core.u_intpipe.wb_rf_waddr;
  assign debug_rf_wdata_o = u_dut.u_core.u_intpipe.wb_rf_wdata;
  assign debug_rf_thread_id_o = u_dut.u_core.u_intpipe.wb_rf_thread_id;
  assign debug_rf_x1_o = u_dut.u_core.u_intpipe.rf.u_rf.rf_q[1];

  assign debug_id_valid_o = u_dut.u_core.u_intpipe.id_valid;
  assign debug_id_valid_qual_o = u_dut.u_core.u_intpipe.id_valid_qual;
  assign debug_id_ctrl_stall_o = u_dut.u_core.u_intpipe.id_ctrl_stall;
  assign debug_id_take_pc_o = u_dut.u_core.u_intpipe.id_take_pc;
  assign debug_id_pc_o = u_dut.u_core.u_intpipe.id_pc;
  assign debug_id_inst_o = u_dut.u_core.u_intpipe.id_inst_bits;
  assign debug_id_raddr1_o = u_dut.u_core.u_intpipe.id_raddr1;
  assign debug_id_rs1_data_o = u_dut.u_core.u_intpipe.id_reg_data[0];
  assign debug_id_jalr_o = u_dut.u_core.u_intpipe.id_ctrl.jalr;

  assign debug_ex_valid_o = u_dut.u_core.u_intpipe.ex_reg_valid;
  assign debug_ex_pc_o = u_dut.u_core.u_intpipe.ex_reg_pc;
  assign debug_ex_inst_o = u_dut.u_core.u_intpipe.ex_reg_inst;
  assign debug_ex_jalr_o = u_dut.u_core.u_intpipe.ex_ctrl.jalr;
  assign debug_ex_op1_o = u_dut.u_core.u_intpipe.ex_op1;
  assign debug_ex_op2_o = u_dut.u_core.u_intpipe.ex_op2;
  assign debug_ex_alu_out_o = u_dut.u_core.u_intpipe.ex_alu_out;

  assign debug_tag_valid_o = u_dut.u_core.u_intpipe.tag_reg_valid;
  assign debug_tag_take_pc_o = u_dut.u_core.u_intpipe.tag_take_pc;
  assign debug_tag_pc_o = u_dut.u_core.u_intpipe.tag_reg_pc;
  assign debug_tag_inst_o = u_dut.u_core.u_intpipe.tag_reg_inst;
  assign debug_tag_jalr_o = u_dut.u_core.u_intpipe.tag_ctrl.jalr;
  assign debug_tag_reg_wdata_o = u_dut.u_core.u_intpipe.tag_reg_wdata;
  assign debug_tag_int_wdata_o = u_dut.u_core.u_intpipe.tag_int_wdata;
  assign debug_tag_npc_o = u_dut.u_core.u_intpipe.tag_npc;

  assign debug_mem_valid_o = u_dut.u_core.u_intpipe.mem_reg_valid;
  assign debug_mem_pc_o = u_dut.u_core.u_intpipe.mem_reg_pc;
  assign debug_wb_valid_o = u_dut.u_core.u_intpipe.wb_reg_valid;
  assign debug_wb_pc_o = u_dut.u_core.u_intpipe.wb_reg_pc;
  assign debug_id_div_req_o = u_dut.u_core.u_intpipe.id_div_req;
  assign debug_ex_div_req_o = u_dut.u_core.u_intpipe.ex_div_req;
  assign debug_ex_div_ready_o = u_dut.u_core.u_intpipe.ex_div_ready;
  assign debug_wb_div_resp_valid_o = u_dut.u_core.u_intpipe.wb_div_resp_valid;
  assign debug_wb_div_resp_valid_early_o = u_dut.u_core.u_intpipe.wb_div_resp_valid_early;
  assign debug_wb_div_resp_ready_o = u_dut.u_core.u_intpipe.wb_div_resp_ready;
  assign debug_wb_div_wen_o = u_dut.u_core.u_intpipe.wb_div_wen;
  assign debug_wb_div_resp_waddr_o = u_dut.u_core.u_intpipe.wb_div_resp_dest.addr;
  assign debug_wb_div_resp_data_o = u_dut.u_core.u_intpipe.wb_div_resp_data;
  assign debug_id_int_sboard_hazard_o = u_dut.u_core.u_intpipe.id_int_sboard_hazard;

  logic icache_req_valid;

  /* verilator lint_off PINCONNECTEMPTY */
  minion_top #(
    .DebugApbEn(DebugApbEn),
    .DebugMonEn(DebugMonEn),
    .TraceEn(TraceEn),
    .VpuEn(VpuEn)
  ) u_dut (
    .clk_i                      (clk_i),
    .rst_c_ni                   (rst_c_ni),
    .rst_d_ni                   (rst_d_ni),
    .rst_w_ni                   (rst_w_ni),
    .dft_i                      ('0),
    .eco_i                      (10'h3a5),
    .eco_o                      (),
    .ioshire_i                  (1'b0),
    .nsleepin_i                 (nsleepin_i),
    .iso_enable_i               (1'b0),
    .nsleepout_o                (),
    .chicken_bits_i             ('0),
    .l2_dcache_evict_req_ready_i(l2_evict_req_ready_i),
    .l2_dcache_evict_req_valid_o(l2_evict_req_valid_o),
    .l2_dcache_evict_req_o      (l2_evict_req),
    .l2_dcache_miss_req_ready_i (l2_miss_req_ready_i),
    .l2_dcache_miss_req_valid_o (l2_miss_req_valid_o),
    .l2_dcache_miss_req_o       (l2_miss_req),
    .l2_dcache_resp_ready_o     (l2_resp_ready_o),
    .l2_dcache_resp_valid_i     (l2_resp_valid_i),
    .l2_dcache_resp_i           (l2_resp),
    .icache_req_ready_i         (icache_req_ready_i),
    .icache_req_valid_o         (icache_req_valid),
    .icache_req_o               (icache_req),
    .icache_resp_valid_i        (icache_resp_valid_i),
    .icache_resp_miss_i         (1'b0),
    .icache_resp_i              (icache_resp),
    .icache_fill_done_i         (1'b0),
    .icache_flush_data_o        (),
    .satp_info_o                (),
    .matp_info_o                (),
    .tlb_invalidate_o           (),
    .dc_ptw_req_data_o          (),
    .dc_ptw_req_valid_o         (),
    .dc_ptw_req_ready_i         (1'b1),
    .ptw_dc_resp_data_i         ('0),
    .ptw_dc_resp_valid_i        (1'b0),
    .interrupts_i               ('0),
    .shire_id_i                 ('0),
    .shire_min_id_i             ('0),
    .enabled_i                  (enabled_i),
    .reset_vector_i             (reset_vector_i),
    .mprot_i                    ('0),
    .vmspagesize_i              ('0),
    .flb_neigh_req_valid_o      (),
    .flb_neigh_req_data_o       (),
    .flb_neigh_resp_valid_i     (1'b0),
    .flb_neigh_resp_data_i      (1'b0),
    .te_thread_sel_i            (1'b0),
    .trace_encoder_o            (trace_encoder),
    .te_enable_i                (te_enable_i),
    .apb_paddr_i                ('0),
    .apb_penable_i              (1'b0),
    .apb_prdata_o               (),
    .apb_pready_o               (),
    .apb_psel_i                 (1'b0),
    .apb_pslverr_o              (),
    .apb_pwdata_i               ('0),
    .apb_pwrite_i               (1'b0),
    .debug_in_i                 (debug_in),
    .debug_out_o                (debug_out),
    .minion_dbg_signals_o       (minion_dbg_signals),
    .minion_dbg_signals_mux_i   ('0),
    .minion_dbg_sig_enable_i    (1'b0),
    .esr_features_i             (esr_features),
    .esr_bypass_dcache_i        (1'b0),
    .esr_shire_coop_mode_i      (1'b0),
    .esr_minion_mem_override_i  ('0),
    .pmu_count_up_o             (),
    .pmu_read_data_i            ('0),
    .pmu_read_sel_o             (),
    .pmu_write_en_o             (),
    .pmu_write_data_o           (),
    .pmu_neigh_event_sel_o      ()
  );
  /* verilator lint_on PINCONNECTEMPTY */

endmodule
/* verilator lint_on SYNCASYNCNET */

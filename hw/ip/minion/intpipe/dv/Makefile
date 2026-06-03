REPO_ROOT := $(shell git rev-parse --show-toplevel)

TB_TOP  := isa_migration_arch_wb_tb
RTL_SRCS := $(REPO_ROOT)/hw/ip/dft/rtl/dft_pkg.sv \
            $(REPO_ROOT)/hw/ip/etlink/rtl/etlink_pkg.sv \
            $(REPO_ROOT)/hw/ip/minion/rtl/minion_pkg.sv \
            $(REPO_ROOT)/hw/ip/minion/frontend/rtl/minion_frontend_pkg.sv \
            $(REPO_ROOT)/hw/ip/minion/frontend/rtl/intpipe_decode.sv \
            $(REPO_ROOT)/hw/ip/minion/intpipe/rtl/intpipe_alu.sv \
            $(REPO_ROOT)/hw/ip/minion/intpipe/rtl/intpipe_rf.sv \
            $(REPO_ROOT)/hw/ip/tech_generic/prim_rf_2r1w_preview/rtl/prim_rf_2r1w_preview.sv \
            $(REPO_ROOT)/hw/ip/tech_generic/prim_write_preview_en/rtl/prim_write_preview_en.sv \
            $(REPO_ROOT)/hw/ip/tech_generic/prim_write_commit_en/rtl/prim_write_commit_en.sv \
            $(REPO_ROOT)/hw/ip/tech_generic/prim_clk_gate/rtl/prim_clk_gate.sv \
            $(CURDIR)/isa_migration_arch_wb_tb.sv
CC_SRCS := $(CURDIR)/isa_migration_arch_wb_test.cc

LINT_SRCS := $(REPO_ROOT)/hw/ip/dft/rtl/dft_pkg.sv \
             $(REPO_ROOT)/hw/ip/etlink/rtl/etlink_pkg.sv \
             $(REPO_ROOT)/hw/ip/minion/rtl/minion_pkg.sv \
             $(REPO_ROOT)/hw/ip/minion/frontend/rtl/minion_frontend_pkg.sv \
             $(REPO_ROOT)/hw/ip/minion/frontend/rtl/intpipe_decode.sv \
             $(REPO_ROOT)/hw/ip/minion/intpipe/rtl/intpipe_alu.sv \
             $(REPO_ROOT)/hw/ip/minion/intpipe/rtl/intpipe_rf.sv \
             $(REPO_ROOT)/hw/ip/tech_generic/prim_rf_2r1w_preview/rtl/prim_rf_2r1w_preview.sv \
             $(REPO_ROOT)/hw/ip/tech_generic/prim_write_preview_en/rtl/prim_write_preview_en.sv \
             $(REPO_ROOT)/hw/ip/tech_generic/prim_write_commit_en/rtl/prim_write_commit_en.sv \
             $(REPO_ROOT)/hw/ip/tech_generic/prim_clk_gate/rtl/prim_clk_gate.sv \
             $(CURDIR)/isa_migration_arch_wb_tb.sv
LINT_FLAGS := -Wno-MULTITOP -Wno-UNUSEDSIGNAL

include $(REPO_ROOT)/mk/verilator.mk

COMMON_FLAGS += -Wno-UNUSEDSIGNAL -Wno-SIDEEFFECT

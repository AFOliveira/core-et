REPO_ROOT  := $(shell git rev-parse --show-toplevel 2>/dev/null || echo /root/afonso/europeriscv-implementation-fresh/repos/core-et)
COSIM_NAME := intpipe_decode_migration

NEW_RTL  := $(REPO_ROOT)/hw/ip/minion/rtl/minion_pkg.sv \
            $(REPO_ROOT)/hw/ip/minion/frontend/rtl/minion_frontend_pkg.sv \
            $(REPO_ROOT)/hw/ip/minion/frontend/rtl/intpipe_decode.sv
ORIG_RTL := $(ORIG_ROOT)/rtl/shire/minion/intpipe/intpipe_decode.v

ORIG_RENAME := intpipe_decode

include $(REPO_ROOT)/mk/rtlcosim.mk

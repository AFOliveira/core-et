# ACT4 integration for Minion.

REPO_ROOT := $(shell git rev-parse --show-toplevel)

ACT_VERSION ?= 4.0.0
SAIL_VERSION ?= 0.10
XPACK_RISCV_GCC_VERSION ?= 15.2.0-1

ACT_DIR := $(REPO_ROOT)/dv/act
ACT_CONFIG := $(ACT_DIR)/config/minion-rv64imc/test_config.yaml
ACT_EXTENSIONS ?= I,M,Zca,Zicsr
ACT_JOBS ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 1)
ACT_RUN_JOBS ?= $(shell procs=$$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 1); jobs=$$((procs / 2)); if [ $$jobs -lt 1 ]; then jobs=1; fi; echo $$jobs)
ACT_MAX_CYCLES ?= 2000000

ACT_BUILD_DIR ?= $(REPO_ROOT)/build/act
ACT_TOOLS_DIR ?= $(ACT_BUILD_DIR)/tools
ACT_ROOT ?= $(ACT_TOOLS_DIR)/riscv-arch-test-$(ACT_VERSION)
ACT_WORKDIR ?= $(ACT_BUILD_DIR)/work
ACT_VENV ?= $(ACT_TOOLS_DIR)/venv
SAIL_ROOT ?= $(ACT_TOOLS_DIR)/sail-riscv-$(SAIL_VERSION)
RISCV_GCC_ROOT ?= $(ACT_TOOLS_DIR)/xpack-riscv-none-elf-gcc-$(XPACK_RISCV_GCC_VERSION)
RISCV_GCC_SHIM_DIR ?= $(ACT_TOOLS_DIR)/riscv-gcc-shims
ACT_GEM_HOME ?= $(ACT_TOOLS_DIR)/gems
ACT_BUNDLE_APP_CONFIG ?= $(ACT_TOOLS_DIR)/bundle-config
ACT_BUNDLE_PATH ?= $(ACT_TOOLS_DIR)/bundle
ACT_RUBY_VERSION ?= $(shell ruby -rrbconfig -e 'print RbConfig::CONFIG["ruby_version"]')
ACT_BUNDLE_GEM_DIR ?= $(ACT_BUNDLE_PATH)/ruby/$(ACT_RUBY_VERSION)
ACT_BUNDLE_BIN ?= $(ACT_BUNDLE_PATH)/ruby/$(ACT_RUBY_VERSION)/bin
ACT_XDG_DATA_HOME ?= $(ACT_TOOLS_DIR)/xdg-data
ACT_XDG_CACHE_HOME ?= $(ACT_TOOLS_DIR)/xdg-cache
ACT_UDB_Z3_VERSION ?= z3-4.16.0
ACT_UDB_HOST_CPU ?= $(shell ruby -rrbconfig -e 'cpu = RbConfig::CONFIG["host_cpu"]; print(cpu =~ /arm64|aarch64/ ? "arm64" : "x64")')
ACT_UDB_Z3_LIB ?= $(ACT_XDG_CACHE_HOME)/udb/z3/$(ACT_UDB_Z3_VERSION)/$(ACT_UDB_HOST_CPU)/libz3.so

HOST_RISCV64_GCC := $(shell command -v riscv64-unknown-elf-gcc 2>/dev/null)
ifeq ($(HOST_RISCV64_GCC),)
  ACT_RISCV_TOOL_DEPS := $(RISCV_GCC_SHIM_DIR)/riscv64-unknown-elf-gcc
  ACT_RISCV_PATH := $(RISCV_GCC_SHIM_DIR)
else
  ACT_RISCV_TOOL_DEPS :=
  ACT_RISCV_PATH := $(dir $(HOST_RISCV64_GCC))
endif

ACT_RUBY_ENV := GEM_HOME="$(ACT_GEM_HOME)" GEM_PATH="$(ACT_GEM_HOME):$(ACT_BUNDLE_GEM_DIR)" BUNDLE_APP_CONFIG="$(ACT_BUNDLE_APP_CONFIG)" BUNDLE_PATH="$(ACT_BUNDLE_PATH)" XDG_DATA_HOME="$(ACT_XDG_DATA_HOME)" XDG_CACHE_HOME="$(ACT_XDG_CACHE_HOME)"
ACT_PATH := $(ACT_GEM_HOME)/bin:$(ACT_BUNDLE_BIN):$(ACT_VENV)/bin:$(SAIL_ROOT)/bin:$(ACT_RISCV_PATH):$(PATH)
ACT_ELF_DIR := $(ACT_WORKDIR)/minion-rv64imc/elfs
MINION_ACT_SIM := $(REPO_ROOT)/hw/ip/minion/dv/build/obj_minion_top_act/Vminion_top_act_tb

.PHONY: test tools elfs run sim clean

test: tools sim elfs run

tools: $(ACT_ROOT)/Makefile $(ACT_VENV)/bin/act $(SAIL_ROOT)/bin/sail_riscv_sim $(ACT_GEM_HOME)/bin/bundle $(ACT_UDB_Z3_LIB) $(ACT_RISCV_TOOL_DEPS)

sim:
	$(MAKE) --no-print-directory -C $(REPO_ROOT)/hw/ip/minion/dv act-sim

elfs: tools
	@mkdir -p $(ACT_XDG_DATA_HOME) $(ACT_XDG_CACHE_HOME)
	$(ACT_RUBY_ENV) PATH="$(ACT_PATH)" $(MAKE) --no-print-directory -C $(ACT_ROOT) \
	  CONFIG_FILES="$(ACT_CONFIG)" \
	  WORKDIR="$(ACT_WORKDIR)" \
	  EXTENSIONS="$(ACT_EXTENSIONS)" \
	  FAST=True \
	  JOBS=$(ACT_JOBS) \
	  -j$(ACT_JOBS)

run: sim elfs
	$(ACT_DIR)/scripts/run_act.py \
	  --sim "$(MINION_ACT_SIM)" \
	  --elf-dir "$(ACT_ELF_DIR)" \
	  --extensions "$(ACT_EXTENSIONS)" \
	  --jobs "$(ACT_RUN_JOBS)" \
	  --max-cycles "$(ACT_MAX_CYCLES)"

$(ACT_ROOT)/Makefile:
	@mkdir -p $(ACT_TOOLS_DIR)
	git clone --depth 1 --branch $(ACT_VERSION) https://github.com/riscv/riscv-arch-test.git $(ACT_ROOT)

$(ACT_VENV)/bin/act: $(ACT_ROOT)/Makefile
	python3 -m venv $(ACT_VENV)
	$(ACT_VENV)/bin/python -m pip install --upgrade pip setuptools wheel
	$(ACT_VENV)/bin/python -m pip install -e $(ACT_ROOT)/framework -e $(ACT_ROOT)/generators/testgen -e $(ACT_ROOT)/generators/coverage

$(ACT_GEM_HOME)/bin/bundle:
	@mkdir -p $(ACT_GEM_HOME) $(ACT_BUNDLE_APP_CONFIG) $(ACT_BUNDLE_PATH) $(ACT_XDG_DATA_HOME) $(ACT_XDG_CACHE_HOME)
	$(ACT_RUBY_ENV) gem install bundler -v 4.0.8 --no-document

$(ACT_UDB_Z3_LIB): $(ACT_ROOT)/Makefile $(ACT_GEM_HOME)/bin/bundle
	@mkdir -p $(ACT_XDG_DATA_HOME) $(ACT_XDG_CACHE_HOME)
	$(ACT_RUBY_ENV) PATH="$(ACT_PATH)" bundle install --gemfile="$(ACT_ROOT)/framework/src/act/data/Gemfile"
	@udb_ext=$$(find "$(ACT_BUNDLE_GEM_DIR)/gems" -path '*/ext/udb_download/extconf.rb' | sort | tail -1); \
	if [ -z "$$udb_ext" ]; then echo "Could not find UDB extconf.rb after bundle install" >&2; exit 1; fi; \
	cd "$$(dirname "$$udb_ext")" && $(ACT_RUBY_ENV) PATH="$(ACT_PATH)" ruby extconf.rb

$(SAIL_ROOT)/bin/sail_riscv_sim:
	@mkdir -p $(ACT_TOOLS_DIR)
	@tmp=$$(mktemp -d $(ACT_TOOLS_DIR)/sail-download.XXXXXX); \
	curl -fL https://github.com/riscv/sail-riscv/releases/download/$(SAIL_VERSION)/sail-riscv-Linux-x86_64.tar.gz -o $$tmp/sail-riscv.tar.gz; \
	mkdir -p $(SAIL_ROOT); \
	tar -xzf $$tmp/sail-riscv.tar.gz -C $(SAIL_ROOT) --strip-components=1; \
	rm -rf $$tmp
	$(SAIL_ROOT)/bin/sail_riscv_sim --version

$(RISCV_GCC_SHIM_DIR)/riscv64-unknown-elf-gcc:
	@mkdir -p $(ACT_TOOLS_DIR) $(RISCV_GCC_ROOT) $(RISCV_GCC_SHIM_DIR)
	@tmp=$$(mktemp -d $(ACT_TOOLS_DIR)/riscv-gcc-download.XXXXXX); \
	curl -fL https://sourceforge.net/projects/riscv-none-elf-gcc-xpack/files/v$(XPACK_RISCV_GCC_VERSION)/xpack-riscv-none-elf-gcc-$(XPACK_RISCV_GCC_VERSION)-linux-x64.tar.gz/download -o $$tmp/riscv-gcc.tar.gz; \
	tar -xzf $$tmp/riscv-gcc.tar.gz -C $(RISCV_GCC_ROOT) --strip-components=1; \
	rm -rf $$tmp
	ln -sf $(RISCV_GCC_ROOT)/bin/riscv-none-elf-gcc $(RISCV_GCC_SHIM_DIR)/riscv64-unknown-elf-gcc
	ln -sf $(RISCV_GCC_ROOT)/bin/riscv-none-elf-objdump $(RISCV_GCC_SHIM_DIR)/riscv64-unknown-elf-objdump
	ln -sf $(RISCV_GCC_ROOT)/bin/riscv-none-elf-nm $(RISCV_GCC_SHIM_DIR)/riscv64-unknown-elf-nm
	$(RISCV_GCC_SHIM_DIR)/riscv64-unknown-elf-gcc --version | head -1

clean:
	rm -rf $(ACT_WORKDIR)

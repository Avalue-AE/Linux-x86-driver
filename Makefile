
# ======================= Avalue Technology Co., Ltd. ====================
# Avalue Driver Makefile
# ========================================================================
#
# Build kernel modules for one Avalue board, driven entirely by that board's
# configuration file (configs/boards/<BOARD_NAME>.conf).
#
# Quick start (run on the target board; BOARD_NAME is auto-detected):
#   make help                          # list every target and variable
#   make                               # build all subsystems the board enables
#   make gpio                          # build just one subsystem
#   sudo make install
#
# Guidelines:
#   * BOARD_NAME auto-detects from /sys/class/dmi/id/board_name, so normally
#     you do not pass it. Override it only in special cases -- e.g. building
#     for another board off-target. It must match a configs/boards/*.conf file.
#   * A board only builds the subsystems its .conf declares (MAKE_*_DEVICE).
#     Requesting one it does not have stops early with a clear message.
#   * The board header src/configs/board.h is generated from the .conf on
#     demand; `make clean` removes it. Re-run after editing the .conf.
# ========================================================================

KERNEL_VERSION := $(shell uname -r)
KERNEL_SOURCE := /lib/modules/$(KERNEL_VERSION)/build
PWD := $(shell pwd)
CC := gcc
AR := ar rsc

# Goals that do not build anything for a specific board and therefore must
# not require (or include) a board config: `make help`, `make clean`.
# A bare `make` (no goal) still needs a board.
NONBUILD_GOALS := clean help
ifneq ($(MAKECMDGOALS),)
    ifeq ($(filter-out $(NONBUILD_GOALS),$(MAKECMDGOALS)),)
        SKIP_BOARD_CONFIG := 1
    endif
endif

ifndef SKIP_BOARD_CONFIG
	BOARD_NAME ?= $(shell cat /sys/class/dmi/id/board_name 2>/dev/null | sed 's/[^a-zA-Z0-9_-].*//')
	BOARD_CONFIG := configs/boards/$(BOARD_NAME).conf
	BOARD_HEADER := src/configs/board.h

    ifeq ($(wildcard $(BOARD_CONFIG)),)
        $(error Config file '$(BOARD_CONFIG)' not found! Create it at configs/boards/$(BOARD_NAME).conf -- see README.md's Board Configuration section and scripts/config.sh)
    endif
    include $(BOARD_CONFIG)
endif

# ========================= Driver Configurations ============================

DRIVERS := watchdog gpio hwmon misc

CFG_watchdog := MAKE_WDT
CFG_gpio := MAKE_GPIO
CFG_hwmon := MAKE_HWM
CFG_misc := MAKE_MISC

KO_watchdog := wdt.ko
KO_gpio := gpio.ko
KO_hwmon := hwm.ko
KO_misc := misc.ko

# A "<subsystem>-debug" goal must select exactly that one subsystem, the
# same as its plain counterpart -- only CONFIG_DEBUG differs. Without this,
# "hwmon-debug" does not match $(DRIVERS) in the filter below, so it falls
# through to the "no subsystem named" branch and silently builds every
# subsystem the board declares instead of the one named on the command line.
DEBUG_GOALS := $(filter %-debug,$(MAKECMDGOALS))
DEBUG_DRIVERS := $(patsubst %-debug,%,$(DEBUG_GOALS))
BAD_DEBUG_GOALS := $(filter-out $(DRIVERS),$(DEBUG_DRIVERS))
ifneq ($(BAD_DEBUG_GOALS),)
    $(error '$(addsuffix -debug,$(BAD_DEBUG_GOALS))' is not a valid debug target -- valid subsystems are: $(DRIVERS))
endif

TARGET_DRIVERS := $(filter $(DRIVERS), $(MAKECMDGOALS) $(DEBUG_DRIVERS))

ifneq ($(TARGET_DRIVERS),)
    # Create variables for default 'n'
    $(foreach drv, $(DRIVERS), \
        $(eval $(CFG_$(drv)) := n) \
    )

    # Set selected drivers to 'm'
    $(foreach drv, $(TARGET_DRIVERS), \
        $(eval $(CFG_$(drv)) := m) \
    )

    # Guard: a driver was requested for a board whose .conf does not
    # configure that subsystem (no MAKE_<X>_DEVICE key). Without this the
    # kernel build fails with a cryptic 'No rule to make target src/hal//.o'.
    # A .conf can also set the device key and leave the matching chipset key
    # empty -- Kbuild then composes 'src/hal/<device>/.o', the same cryptic
    # failure, so check both keys before letting the driver through.
    $(foreach drv, $(TARGET_DRIVERS), \
        $(if $($(CFG_$(drv))_DEVICE), \
            $(if $($(CFG_$(drv))_CHIPSET),, \
                $(error Board '$(BOARD_NAME)' has '$(drv)' device '$($(CFG_$(drv))_DEVICE)' but no chipset: $(BOARD_CONFIG) leaves $(CFG_$(drv))_CHIPSET empty -- set it to the chip this board uses for $(drv), nothing to build)), \
            $(error Board '$(BOARD_NAME)' has no '$(drv)' subsystem: $(BOARD_CONFIG) defines no $(CFG_$(drv))_DEVICE -- this board does not support $(drv), nothing to build)))
else ifndef SKIP_BOARD_CONFIG
    # No subsystem named on the command line (plain `make`, `make modules`,
    # `make install`, ...). Enable exactly the subsystems the board's .conf
    # declares (has a MAKE_<X>_DEVICE key) -- otherwise none of MAKE_WDT /
    # MAKE_GPIO / MAKE_HWM / MAKE_MISC is ever set to 'm', obj-m stays
    # empty, and the build silently produces zero modules while still
    # exiting 0. A device key with an empty chipset key is the same
    # incomplete block -- warn and skip just that subsystem instead of
    # failing the whole build, so the board still gets what it does declare.
    $(foreach drv, $(DRIVERS), \
        $(eval $(CFG_$(drv)) := $(if $($(CFG_$(drv))_DEVICE), \
            $(if $($(CFG_$(drv))_CHIPSET),m, \
                $(warning Board '$(BOARD_NAME)' has '$(drv)' device '$($(CFG_$(drv))_DEVICE)' but no chipset: $(BOARD_CONFIG) leaves $(CFG_$(drv))_CHIPSET empty -- skipping $(drv))n), \
            n)) \
    )
endif

export BOARD_NAME
export MAKE_WDT
export MAKE_GPIO
export MAKE_HWM
export MAKE_MISC

# ========================= Build Targets ================================

.PHONY: all clean modules install uninstall help $(DRIVERS)

.DEFAULT_GOAL := all

all: modules

help:
	@echo "Avalue Driver - build kernel modules for one board from its .conf"
	@echo ""
	@echo "Usage: make [BOARD_NAME=<board>] <target>"
	@echo ""
	@echo "  BOARD_NAME   Auto-detected from /sys/class/dmi/id/board_name, so you"
	@echo "               normally omit it. Pass it only to override the board"
	@echo "               (special case); must match configs/boards/<BOARD_NAME>.conf."
	@echo ""
	@echo "Build targets:"
	@echo "  all | modules      Build every subsystem the board enables (default)."
	@echo "  watchdog           Build only wdt.ko."
	@echo "  gpio               Build only gpio.ko."
	@echo "  hwmon              Build only hwm.ko."
	@echo "  misc               Build only misc.ko."
	@echo "  <subsystem>-debug  Build one subsystem with CONFIG_DEBUG=y (verbose),"
	@echo "                     e.g. 'make gpio-debug'."
	@echo ""
	@echo "Lifecycle:"
	@echo "  install            Install and auto-load the built modules (needs sudo)."
	@echo "  uninstall          Unload and remove them (needs sudo)."
	@echo "  clean              Remove build artifacts and the generated board.h."
	@echo ""
	@echo "Helpers:"
	@echo "  help               Show this message."
	@echo ""
	@echo "Examples (on the target board -- BOARD_NAME auto-detected):"
	@echo "  make                    # build all enabled subsystems"
	@echo "  make gpio               # just GPIO"
	@echo "  make gpio-debug         # GPIO with debug logging"
	@echo "  sudo make install       # install and load"
	@echo "  make clean"
	@echo ""
	@echo "  # Special case: override the board (e.g. building off-target)"
	@echo "  make BOARD_NAME=ESM-KX60G gpio"
	@echo ""
	@echo "A board only builds the subsystems its .conf declares (MAKE_*_DEVICE);"
	@echo "asking for one it does not have stops early with a clear message."

config:
	@if [ ! -f $(BOARD_CONFIG) ]; then \
		echo "[MAKEFILE]: Config file '$(BOARD_CONFIG)' not found! Create it at configs/boards/$(BOARD_NAME).conf -- see README.md's Board Configuration section and scripts/config.sh"; \
		exit 1; \
	fi

	@# Regenerate the header when it is missing, when the .conf (or the
	@# generator) is newer than it, or when it was generated for a different
	@# board -- otherwise a stale header silently hides .conf edits or a
	@# BOARD_NAME switch and the build fails on undefined CONFIG_* macros.
	@if [ ! -f $(BOARD_HEADER) ] \
	    || [ $(BOARD_CONFIG) -nt $(BOARD_HEADER) ] \
	    || [ scripts/config.sh -nt $(BOARD_HEADER) ] \
	    || ! grep -qF '#define CONFIG_BOARD_NAME "$(BOARD_NAME)"' $(BOARD_HEADER); then \
		echo "[MAKEFILE]: Generating board header '$(BOARD_HEADER)' from '$(BOARD_CONFIG)'"; \
		scripts/config.sh $(BOARD_CONFIG) $(BOARD_HEADER); \
	fi

modules: config
	@echo "[MAKEFILE]: Building for Board: $(BOARD_NAME)"
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules

$(DRIVERS): config
	@echo "[MAKEFILE]: Building driver module: $@ for Board: $(BOARD_NAME)"
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) $(CFG_$@)=m modules

%-debug: config
	@echo "[MAKEFILE]: Building driver module: $* with DEBUG for Board: $(BOARD_NAME)"
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) $(CFG_$*)=m CONFIG_DEBUG=y modules

clean:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) clean
	@if [ -f src/configs/board.h ]; then \
		echo "[MAKEFILE]: Removing generated board header 'src/configs/board.h'"; \
		rm -f src/configs/board.h; \
	fi

# ========================= Install ================================

MODULE_CONF := /etc/modules-load.d/$(BOARD_NAME).conf

install:
	@echo "[MAKEFILE]: Installing BUILT modules..."
	
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) \
		INSTALL_MOD_DIR=extra/$(BOARD_NAME) \
		modules_install

	@echo "[MAKEFILE]: Updating /etc/modules-load.d/..."
	@if [ ! -f $(MODULE_CONF) ]; then echo "# Module load configuration for $(BOARD_NAME)" > $(MODULE_CONF); fi

	@$(foreach drv, $(DRIVERS), \
		KO_PATH=$(shell find . -name $(KO_$(drv)) 2>/dev/null); \
		if [ -n "$$KO_PATH" ]; then \
			MODULE_NAME=$(basename $(KO_$(drv))); \
			echo "    Processing $$MODULE_NAME (Found $$KO_PATH)"; \
			if ! grep -Fq "$$MODULE_NAME" $(MODULE_CONF); then \
				echo "$$MODULE_NAME" >> $(MODULE_CONF); \
				echo "    [+]: Added to auto-load"; \
			else \
				echo "    [-]: Already in auto-load list"; \
			fi; \
		fi; \
	)

	@echo "[MAKEFILE]: Updating dependency map..."
	@depmod -a


	@echo "[MAKEFILE]: Loading modules now..."
	@$(foreach drv, $(DRIVERS), \
		if [ "$($(CFG_$(drv)))" = "m" ]; then \
			MODULE_NAME=$(basename $(KO_$(drv))); \
			modprobe -r $$MODULE_NAME || exit 1; \
			if modprobe $$MODULE_NAME; then \
				echo "    [+] Loaded: $$MODULE_NAME"; \
			else \
				echo "    [!] Failed to load: $$MODULE_NAME"; \
			fi; \
		fi; \
	)

	@echo "[MAKEFILE]: Installation complete."

# ========================= Uninstall ==============================

uninstall:
	@echo "[MAKEFILE]: Unloading modules now..."
	@$(foreach drv, $(DRIVERS), \
		if [ "$($(CFG_$(drv)))" = "m" ]; then \
			MODULE_NAME=$(basename $(KO_$(drv))); \
			echo "	Processing $$MODULE_NAME"; \
			if lsmod | grep -q "^$$MODULE_NAME"; then \
				modprobe -r $$MODULE_NAME || exit 1; \
				echo "    [-] Unloaded: $$MODULE_NAME"; \
			else \
				echo "    [.] Not loaded: $$MODULE_NAME (skip)"; \
			fi; \
		fi; \
	)

	@echo "[MAKEFILE]: Uninstalling modules..."
	@TARGET_DIR="/lib/modules/$(KERNEL_VERSION)/extra/$(BOARD_NAME)"; \
	if [ -d "$$TARGET_DIR" ]; then \
		rm -rf "$$TARGET_DIR"; \
		echo "    [-] Removed directory: $$TARGET_DIR"; \
	else \
		echo "    [!] Directory not found: $$TARGET_DIR"; \
	fi

	@echo "[MAKEFILE]: Removing configuration..."
	@if [ -f $(MODULE_CONF) ]; then \
		rm -f $(MODULE_CONF); \
		echo "    [-] Removed config: $(MODULE_CONF)"; \
	else \
		echo "    [!] Config file not found"; \
	fi

	@echo "[MAKEFILE]: Updating dependency map..."
	@depmod -a
	@echo "[MAKEFILE]: Uninstallation complete."
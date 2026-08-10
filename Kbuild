# ========================= Board Configuration =========================
BOARD_CONFIG := $(M)/configs/boards/$(BOARD_NAME).conf
BOARD_HEADER := $(M)/src/configs/board.h

ifneq ($(BOARD_NAME),)
ifeq ($(wildcard $(BOARD_CONFIG)),)
$(error Config file '$(BOARD_CONFIG)' not found! Please run 'make BOARD_NAME=xxx')
endif
include $(BOARD_CONFIG)
endif

# ========================= Generate Board Header =======================

PHONY += $(BOARD_HEADER)

ccflags-y += -I$(src)/src
ccflags-y += -include $(BOARD_HEADER)

# Ensure all object files depend on the generated header
$(obj)/src/hal/sio/sio.o: $(BOARD_HEADER)
$(obj)/src/hal/ec/ec.o: $(BOARD_HEADER)


# ========================= Compiler Flags ============================
ccflags-y += -Wall -Wextra -Werror
ccflags-y += -Wno-error=type-limits # kernel's own bits.h GENMASK; not ours to fix
ccflags-y += -Wno-unused-parameter
ccflags-y += -Wno-unused-variable
ccflags-y += -Wno-unused-function
ccflags-y += -Wno-unused-but-set-variable
ccflags-y += -Wno-missing-field-initializers
ccflags-y += -Wno-format-security
ccflags-y += -Wno-pointer-sign
ccflags-y += -Wno-deprecated-declarations

ifeq ($(CONFIG_DEBUG), y)
    ccflags-y += -DDEBUG
endif

MAKE_WDT ?= n
MAKE_GPIO ?= n
MAKE_HWM ?= n
MAKE_MISC ?= n

# ========================= Driver Modules =========================

# Watchdog driver module
ifeq ($(MAKE_WDT), m)
    HAL_CHIP := src/hal/$(MAKE_WDT_DEVICE)/$(MAKE_WDT_CHIPSET).o
    HAL_CHIP_WDT := src/hal/$(MAKE_WDT_DEVICE)/$(MAKE_WDT_CHIPSET)_wdt.o
    obj-$(MAKE_WDT) += wdt.o
    wdt-y := $(HAL_CHIP) $(HAL_CHIP_WDT) src/drivers/watchdog.o
endif

# GPIO driver module
ifeq ($(MAKE_GPIO),m)
    HAL_CHIP := src/hal/$(MAKE_GPIO_DEVICE)/$(MAKE_GPIO_CHIPSET).o
    HAL_CHIP_GPIO := src/hal/$(MAKE_GPIO_DEVICE)/$(MAKE_GPIO_CHIPSET)_gpio.o
    # SMBus GPIO parts also need a host-controller protocol object (i801 /
    # zhaoxin); EC-native GPIO (ec/ite) has none, so include it only when set.
    HAL_PROTO := $(if $(MAKE_GPIO_PROTOCOL),src/hal/$(MAKE_GPIO_DEVICE)/$(MAKE_GPIO_PROTOCOL).o)
    obj-$(MAKE_GPIO) += gpio.o
    gpio-y := $(HAL_CHIP) $(HAL_PROTO) $(HAL_CHIP_GPIO) src/drivers/gpio.o
endif

# Hardware Monitor driver module
ifeq ($(MAKE_HWM),m)
    HAL_CHIP := src/hal/$(MAKE_HWM_DEVICE)/$(MAKE_HWM_CHIPSET).o
    HAL_CHIP_HWM := src/hal/$(MAKE_HWM_DEVICE)/$(MAKE_HWM_CHIPSET)_hwm.o
    obj-$(MAKE_HWM) += hwm.o
    hwm-y := $(HAL_CHIP) $(HAL_CHIP_HWM) src/drivers/hwmon.o
endif

# Miscellaneous driver module
ifeq ($(MAKE_MISC),m)
    HAL_CHIP := src/hal/$(MAKE_MISC_DEVICE)/$(MAKE_MISC_CHIPSET).o
    HAL_CHIP_MISC := src/hal/$(MAKE_MISC_DEVICE)/$(MAKE_MISC_CHIPSET)_misc.o
    obj-$(MAKE_MISC) += misc.o
    misc-y := $(HAL_CHIP) $(HAL_CHIP_MISC) src/drivers/misc.o
endif
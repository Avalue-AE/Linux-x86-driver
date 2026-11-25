# Reads a board file into make variables, following CONFIG_BOARD_BASE.
#
# The caller sets BOARD_CONFIG to configs/boards/<board>.conf. A board file
# that sets CONFIG_BOARD_BASE="<other board>" shares that board's hardware:
# the base file is read first, then this file again, so a key set in both
# takes this file's value. scripts/config.sh merges the same two files in the
# same precedence for the header, so make and the header agree on every key.
# Only one level. Make stops on the same three faults config.sh refuses, since
# make skips config.sh while the header is newer than both files: a file that
# names itself, a missing base, and a base that names a base of its own.

include $(BOARD_CONFIG)

BOARD_BASE := $(subst ",,$(strip $(CONFIG_BOARD_BASE)))
ifneq ($(BOARD_BASE),)
    BOARD_BASE_CONFIG := $(dir $(BOARD_CONFIG))$(BOARD_BASE).conf
    ifeq ($(BOARD_BASE),$(basename $(notdir $(BOARD_CONFIG))))
        $(error $(BOARD_CONFIG) sets CONFIG_BOARD_BASE to its own name)
    endif
    ifeq ($(wildcard $(BOARD_BASE_CONFIG)),)
        $(error $(BOARD_CONFIG) sets CONFIG_BOARD_BASE="$(BOARD_BASE)" but $(BOARD_BASE_CONFIG) does not exist)
    endif
    # Cleared before the base is read, so a value left afterwards is the base's own.
    CONFIG_BOARD_BASE :=
    include $(BOARD_BASE_CONFIG)
    ifneq ($(strip $(CONFIG_BOARD_BASE)),)
        $(error $(BOARD_CONFIG) inherits $(BOARD_BASE_CONFIG), which sets CONFIG_BOARD_BASE itself)
    endif
    include $(BOARD_CONFIG)
endif

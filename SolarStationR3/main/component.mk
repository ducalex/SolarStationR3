#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

ULP_APP_NAME ?= ulp_wind
ULP_S_SOURCES = $(COMPONENT_PATH)/ulp/wind.S
ULP_EXP_DEP_OBJECTS := main.o

include $(IDF_PATH)/components/ulp/component_ulp_common.mk

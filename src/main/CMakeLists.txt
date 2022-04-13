set(COMPONENT_SRCS "main.cpp")
set(COMPONENT_ADD_INCLUDEDIRS ".")
register_component()

set(ULP_APP_NAME ulp_wind)
set(ULP_S_SOURCES ulp/wind.S)
set(ULP_EXP_DEP_SRCS "main.cpp")
include(${IDF_PATH}/components/ulp/component_ulp_common.cmake)

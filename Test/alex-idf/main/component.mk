#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_ADD_INCLUDEDIRS += ../components/cfgparser
COMPONENT_ADD_INCLUDEDIRS += ../components/display
COMPONENT_ADD_INCLUDEDIRS += ../components/DHT
COMPONENT_ADD_INCLUDEDIRS += ../components/BMP180

COMPONENT_SRCDIRS := . helpers

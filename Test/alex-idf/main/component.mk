#
# "main" pseudo-component makefile. 
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_ADD_INCLUDEDIRS += ../components/cfgparser
COMPONENT_ADD_INCLUDEDIRS += ../components/display
COMPONENT_ADD_INCLUDEDIRS += ../components/DHT

COMPONENT_SRCDIRS := . helpers

#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_ADD_INCLUDEDIRS += ../components/Arduino
COMPONENT_ADD_INCLUDEDIRS += ../components/ConfigProvider
COMPONENT_ADD_INCLUDEDIRS += ../components/SSD1306Ascii/src
COMPONENT_ADD_INCLUDEDIRS += ../components/DHT
COMPONENT_ADD_INCLUDEDIRS += ../components/BMP180
COMPONENT_ADD_INCLUDEDIRS += ../components/Adafruit_ADS1X15

# I can't get this to work :(
#COMPONENT_ADD_INCLUDEDIRS += $(wildcard $(COMPONENT_PATH)/../components/*)
#COMPONENT_ADD_INCLUDEDIRS += $(wildcard $(COMPONENT_PATH)/../components/*/src)

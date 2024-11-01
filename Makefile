# Makefile Options
# ----------------------------

NAME = midiseq
DESCRIPTION = "Ag C Agon Midi Sequencer"
COMPRESSED = NO

CFLAGS = -Wall -Wextra -Oz -Wint-conversion
CXXFLAGS = -Wall -Wextra -Oz -Wint-conversion

# ----------------------------

include $(shell cedev-config --makefile)
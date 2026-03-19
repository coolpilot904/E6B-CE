NAME = E6B
ICON = icon.png
DESCRIPTION = "E6B Flight Computer"
COMPRESSED = NO

CFLAGS = -Wall -Wextra -Oz
CXXFLAGS = -Wall -Wextra -Oz

include $(shell cedev-config --makefile)

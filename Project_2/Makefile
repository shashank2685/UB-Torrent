


EXES = ubtorrent
EXTRA_CFLAGS = -g 

INCLUDEDIRS = ./include
LDFLAGS = -lpthread


ubtorrent_SRCFILES = error_handlers.c metainfo.c bencode.c ubtorrent.c sha1.c network_util.c download.c

include common.mk

$(EXES) : $(ubtorrent_OBJS)

all: draw.ws

CC       = gcc
WSDIR    = wsServer
INCLUDE  = -I $(WSDIR)/include
CFLAGS   = -Wall
CFLAGS  += $(INCLUDE) -std=c99 -pthread -pedantic
CFLAGS  += `pkg-config --cflags libmypaint-2.0 sqlite3 uuid`
CFLAGS  += -g -lmcheck
LIB      = $(WSDIR)/libws.a
LIB     += `pkg-config --libs libmypaint-2.0 sqlite3 uuid`
LIB     += -lm

DISABLE_VERBOSE = no

# Check if verbose examples
ifeq ($(VERBOSE_EXAMPLES), no)
	CFLAGS += -DDISABLE_VERBOSE
endif

draw.ws: draw.c
	$(CC) $(CFLAGS) $(LDFLAGS) draw.c -o draw.ws $(LIB)

install:
	install draw.ws /usr/local/bin/
	install draw.service /lib/systemd/system/
	install draw /etc/init.d/
	systemctl enable draw && systemctl draw reload
	service draw start
	install 10-proxy-draw.conf /etc/lighttpd/conf-available/
	lighttpd-enable-mod proxy-draw
	service lighttpd force-reload

clean:
	rm -f draw.ws

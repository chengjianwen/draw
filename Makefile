all: draw.ws

CC       = gcc
WSDIR    = $(CURDIR)/../wsServer/
INCLUDE  = -I $(WSDIR)/include
CFLAGS   =  -Wall -Wextra -O2
CFLAGS  +=  $(INCLUDE) -std=c99 -pthread -pedantic
CFLAGS  += `pkg-config --cflags libmypaint json-c`
LIB      =  $(WSDIR)/libws.a
LIB      = `pkg-config --libs libmypaint json-c`
LIB     += -lm

# Check if verbose examples
ifeq ($(VERBOSE_EXAMPLES), no)
	CFLAGS += -DDISABLE_VERBOSE
endif

draw.ws: draw.c
	$(CC) $(CFLAGS) $(LDFLAGS) draw.c -o draw.ws $(LIB)

install:
	install draw.ws /usr/local/bin/
	install draw.service /lib/systemd/system/
	systemctl enable draw
	systemctl start draw
	install 10-proxy-draw.conf /etc/lighttpd/conf-available/
	lighttpd-enable-mod proxy-draw
	systemctl restart lighttpd

clean:
	rm -f draw.ws draw.c

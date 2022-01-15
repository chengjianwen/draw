all: draw.fcgi

CC      = gcc
CFLAGS  = `pkg-config --cflags libmypaint json-c`
LIBS    = `pkg-config --libs libmypaint json-c` -lm -lfcgi

draw.fcgi: draw.o brush.o
	$(CC) -o $@ $^ $(LIBS)

install:
	install draw.fcgi /usr/local/bin/
	systemctl restart lighttpd

clean:
	rm -f draw.fcgi draw.o brush.o

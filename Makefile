
DESTDIR =

xdmdir = /usr/lib/xdm
confdir = /etc/X11/xdm
sessdir = /usr/share/xsessions

CC = gcc

OPT_FLAGS = -O2 -pipe -Wall

ifeq ($(GTK3),1)
CFLAGS += $(OPT_FLAFS) -fPIC $(shell pkg-config --cflags gtk+-3.0)
LIBS += -shared $(shell pkg-config --libs gtk+-3.0)
else
CFLAGS += $(OPT_FLAFS) -fPIC $(shell pkg-config --cflags gtk+-2.0)
LIBS += -shared $(shell pkg-config --libs gtk+-2.0)
endif

ifeq ($(PAM),1)
CFLAGS += -DUSE_PAM
LIBS += -lpam
endif

all: libGtkGreeter.so

libGtkGreeter.so: greet.c greet.h
	$(CC) -o $@ $(CFLAGS) -DSESSIONDIR="\"$(sessdir)\"" -DSYSCONFDIR="\"$(confdir)\"" $(LIBS) $^

install:
	install -m 0755 -D libGtkGreeter.so $(DESTDIR)$(xdmdir)/libGtkGreeter.so
	install -m 0644 -D greeter.conf $(DESTDIR)$(confdir)/greeter.conf

clean:
	rm -rf libGtkGreeter.so *~

dist:
	tar -Jvcf greeter.tar.xz greet.c greet.h greeter.conf Makefile README.md

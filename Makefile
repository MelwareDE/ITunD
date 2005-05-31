#
# Makefile for ISDN Tunnel Daemon
#

VERSION=0.3

PROG=itund

LIBS=-lcapi20 -lz

FILES=isdntun.o tun.o capiconn.o debug.o options.o \
	capi.o signals.o data.o

CFLAGS=-Wall -O2 -I. -DVERSION=\"$(VERSION)\"

DESTDIR=/
BINDIR=sbin

all: $(PROG)
	@echo "Done!"


.SUFFIXES:
.SUFFIXES: .c .o 

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $*.c

$(PROG): $(FILES)
	$(CC) -o $(PROG) $(FILES) $(LDFLAGS) $(LIBS)

clean:
	@rm -f *.o $(PROG) *.tgz  

install: $(PROG)
	mkdir -p $(DESTDIR)/$(BINDIR)
	install -m 0755 $(PROG) $(DESTDIR)/$(BINDIR)/$(PROG)

release:
	@rm -rf $(PROG)-$(VERSION)
	@mkdir -p $(PROG)-$(VERSION)
	@cp *.h *.c $(PROG)-$(VERSION)
	@cp README Makefile COPYING CHANGES $(PROG)-$(VERSION)
	@tar czf $(PROG)-$(VERSION).tgz $(PROG)-$(VERSION)
	@rm -rf $(PROG)-$(VERSION)
	@echo Archive created.

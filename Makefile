.SUFFIXES: .bin .prg
.PRECIOUS:	%.ngd %.ncd %.twx vivado/%.xpr bin/%.bit bin/%.mcs bin/%.M65 bin/%.BIN

ASSETS=		assets
SRCDIR=		src
BINDIR=		bin
B65DIR=		bin65
EXAMPLEDIR=	$(SRCDIR)/examples
UTILDIR=	$(SRCDIR)/utilities
TESTDIR=	$(SRCDIR)/tests
LIBEXECDIR=	libexec

CC65=  cc65
CA65=  ca65 --cpu 4510
LD65=  ld65 -t none
CL65=  cl65 --config src/tests/vicii.cfg

TCPSRCS=	src/arp.c src/checksum.c src/eth.c src/main.c src/nwk.c src/socket.c src/task.c src/dns.c


tcptest.prg:       $(TCPSRCS)
	git submodule init
	git submodule update
	$(CL65) -I $(SRCDIR)/mega65-libc/cc65/include -I include -O -o $*.prg --mapfile $*.map $(TCPSRCS)  $(SRCDIR)/mega65-libc/cc65/src/*.c $(SRCDIR)/mega65-libc/cc65/src/*.s


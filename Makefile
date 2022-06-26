.SUFFIXES: .bin .prg
.PRECIOUS:	%.ngd %.ncd %.twx vivado/%.xpr bin/%.bit bin/%.mcs bin/%.M65 bin/%.BIN

ASSETS=		assets
CONTENTDIR=	content
SRCDIR=		src
BINDIR=		bin
B65DIR=		bin65
EXAMPLEDIR=	$(SRCDIR)/examples
UTILDIR=	$(SRCDIR)/utilities
TESTDIR=	$(SRCDIR)/tests
LIBEXECDIR=	libexec

SUBDEPENDS=	mega65-tools/bin/md2h65 \
		mega65-tools/bin/asciifont.bin
MD2H65=		mega65-tools/bin/md2h65

CC65=  cc65
CA65=  ca65 --cpu 4510
LD65=  ld65 -t none
CL65=  cl65 --config src/tests/vicii.cfg

KICKC= ../kickc/bin/kickc.sh

TCPSRCS=	src/arp.c src/checksum.c src/eth.c src/nwk.c src/socket.c src/task.c src/dns.c src/dhcp.c

all:	fetch-loader.prg fetchm.prg fetchh65.prg haustierbegriff.prg ethtest.prg pages

dist:	all
	mkdir -p sdcard-files
	cp mega65-tools/bin/asciifont.bin sdcard-files/FETCHFNT.M65
	cp fetch.prg sdcard-files/FETCHM.M65
	cp haustierbegriff.prg bbs-client.prg
	if [ -e sdcard-files/FETCH.D81 ];then rm sdcard-files/FETCH.D81 ;fi
	cbmconvert -D8 sdcard-files/FETCH.D81 fetch-loader.prg bbs-client.prg

distpush:	dist
	m65ftp -c 'put sdcard-files/FETCH.D81' -c 'put sdcard-files/FETCHFNT.M65' -c 'put sdcard-files/FETCHM.M65' -c 'quit'

$(SUBDEPENDS):
	git submodule init
	git submodule update
	( cd mega65-tools; make bin/md2h65 )

hex2pcap:	src/hex2pcap.c Makefile
	gcc -Wall -g -o hex2pcap src/hex2pcap.c

uploadpages: pages
	( cd content ; ( echo "prompt" ; echo "mput *" ) | ftp f.mega65.org )

pages:	$(SUBDEPENDS) assets/*
	mkdir -p $(CONTENTDIR)
	@for f in $(shell ls ${ASSETS}/*.md); do echo $$f; ( cd $(ASSETS) ; ../$(MD2H65) ../$${f} ../$(CONTENTDIR)/`basename -s md $${f}`h65 ); done

log2pcap: src/log2pcap.c
	gcc -g -Wall -o log2pcap src/log2pcap.c

fetchm.prg:       $(TCPSRCS) src/fetchm.c src/helper.s
	git submodule init
	git submodule update
	$(CL65) -I $(SRCDIR)/mega65-libc/cc65/include -I include -O -o fetch.prg --mapfile $*.map $(TCPSRCS) src/fetchm.c  $(SRCDIR)/mega65-libc/cc65/src/*.c $(SRCDIR)/mega65-libc/cc65/src/*.s src/helper.s

fetchh65.prg:       $(TCPSRCS) src/fetchh65.c src/helper.s
	git submodule init
	git submodule update
	$(CL65) -I $(SRCDIR)/mega65-libc/cc65/include -I include -O -o fetch.prg --mapfile $*.map $(TCPSRCS) src/fetchh65.c  $(SRCDIR)/mega65-libc/cc65/src/*.c $(SRCDIR)/mega65-libc/cc65/src/*.s src/helper.s

fetch-loader.prg:       src/fetch_loader.c src/helper.s
	$(CL65) -I $(SRCDIR)/mega65-libc/cc65/include -I include -O -o fetch-loader.prg --mapfile $*.map src/fetch_loader.c  $(SRCDIR)/mega65-libc/cc65/src/*.c $(SRCDIR)/mega65-libc/cc65/src/*.s src/helper.s


ethtest.prg:       $(TCPSRCS) src/ethtest.c src/helper.s
	git submodule init
	git submodule update
	$(CL65) -DENABLE_ICMP=1 -I $(SRCDIR)/mega65-libc/cc65/include -I include -O -o ethtest.prg --mapfile $*.map $(TCPSRCS) src/ethtest.c src/helper.s $(SRCDIR)/mega65-libc/cc65/src/*.c $(SRCDIR)/mega65-libc/cc65/src/*.s

fetchkc.prg:       $(TCPSRCS) src/fetch.c
	git submodule init
	git submodule update
	$(KICKC) -t mega65_c64 -a -I $(SRCDIR)/mega65-libc/kickc/include -I include -L src -L $(SRCDIR)/mega65-libc/kickc/src src/fetch.c

haustierbegriff.prg:       $(TCPSRCS) src/haustierbegriff.c
	git submodule init
	git submodule update
	$(CL65) -I $(SRCDIR)/mega65-libc/cc65/include -I include -O -o haustierbegriff.prg --mapfile $*.map $(TCPSRCS) src/haustierbegriff.c  $(SRCDIR)/mega65-libc/cc65/src/*.c $(SRCDIR)/mega65-libc/cc65/src/*.s


.SUFFIXES: .bin .prg
.PRECIOUS:	%.ngd %.ncd %.twx vivado/%.xpr bin/%.bit bin/%.mcs bin/%.M65 bin/%.BIN

USBPORT=	/dev/ttyUSB0

ASSETS=		assets
SRCDIR=		src
BINDIR=		bin
B65DIR=		bin65
EXAMPLEDIR=	$(SRCDIR)/examples
UTILDIR=	$(SRCDIR)/utilities
TESTDIR=	$(SRCDIR)/tests
LIBEXECDIR=	libexec

CONTENTDIR=	content
SDCARDFILESDIR = sdcard-files

SUBDEPENDS=	mega65-tools/bin/md2h65 \
		mega65-tools/bin/asciifont.bin
MD2H65=		../mega65-tools/bin/md2h65
#MD2H65=		md2h65
CBMCONVERT = cbmconvert
M65 = m65
M65FTP = mega65_ftp

CC65=  cc65
CA65=  ca65 --cpu 4510
LD65=  ld65 -t none
CL65=  cl65 --config src/tests/vicii.cfg

KICKC= ../kickc/bin/kickc.sh

TCPSRCS=	src/arp.c src/checksum.c src/eth.c src/nwk.c src/socket.c src/task.c src/dns.c src/dhcp.c

PRGS= fetch.prg fetchm.prg fetcherr.prg fetchh65.prg haustierbegriff.prg ethtest.prg
MAPS= $(subst prg,map,$(PRGS))

all:	$(PRGS) pages

clean:
	rm -rf $(CONTENTDIR)
	rm -rf $(SDCARDFILESDIR)
	rm -f $(PRGS)
	rm -f bbs-client.prg
	rm -f $(MAPS)

dist:	all
	mkdir -p $(SDCCARDFILESDIR)
	cp mega65-tools/bin/asciifont.bin $(SDCCARDFILESDIR)/FETCHFNT.M65
	cp fetchm.prg $(SDCCARDFILESDIR)/FETCHM.M65
	cp fetcherr.prg $(SDCCARDFILESDIR)/FETCHERR.M65
	cp fetchh65.prg $(SDCCARDFILESDIR)/FETCHH65.M65
	cp haustierbegriff.prg bbs-client.prg
	if [ -e $(SDCCARDFILESDIR)/FETCH.D81 ];then rm $(SDCCARDFILESDIR)/FETCH.D81 ;fi
	$(CBMCONVERT) -D8 $(SDCCARDFILESDIR)/FETCH.D81 fetch.prg bbs-client.prg

distpush:	dist
	$(M65) -F ; $(M65FTP) -l $(USBPORT) -c "put $(SDCCARDFILESDIR)/FETCH.D81" -c "put $(SDCCARDFILESDIR)/FETCHM.M65" -c "put $(SDCCARDFILESDIR)/FETCHFNT.M65" -c "put $(SDCCARDFILESDIR)/FETCHH65.M65" -c "put $(SDCCARDFILESDIR)/FETCHERR.M65" -c "quit"

distrun:	distpush
	$(M65) -F -4 -r fetch.prg

distfastrun:	dist
	$(M65) -F ; $(M65FTP) -l $(USBPORT) -c "put $(SDCCARDFILESDIR)/FETCHM.M65" -c "put $(SDCCARDFILESDIR)/FETCHFNT.M65" -c "put $(SDCCARDFILESDIR)/FETCHH65.M65" -c "put $(SDCCARDFILESDIR)/FETCHERR.M65" -c "quit"
	$(M65) -F -4 -r fetch.prg

$(SUBDEPENDS):
	git submodule init
	git submodule update
	( cd mega65-tools; make bin/md2h65 )

hex2pcap:	src/hex2pcap.c Makefile
	gcc -Wall -g -o hex2pcap src/hex2pcap.c

raw2pcap:	src/raw2pcap.c Makefile
	gcc -Wall -g -o raw2pcap src/raw2pcap.c

uploadpages: pages
	( cd content ; ( echo "prompt" ; echo "mput *" ) | ftp f.mega65.org )

pages:	$(SUBDEPENDS) assets/*
	mkdir -p $(CONTENTDIR)
	@for f in $(shell ls ${ASSETS}/*.md); do echo $$f; ( cd $(ASSETS) ; $(MD2H65) ../$${f} ../$(CONTENTDIR)/`basename -s md $${f}`h65 ); done

log2pcap: src/log2pcap.c
	gcc -g -Wall -o log2pcap src/log2pcap.c

fetchm.prg:       src/fetchm.c src/helper.s include/shared_state.h
	git submodule init
	git submodule update
	$(CL65) -I $(SRCDIR)/mega65-libc/cc65/include -I include -O -o $@ --mapfile $*.map src/fetchm.c  $(SRCDIR)/mega65-libc/cc65/src/*.c $(SRCDIR)/mega65-libc/cc65/src/*.s src/helper.s

fetcherr.prg:       src/fetcherr.c src/helper.s include/shared_state.h
	git submodule init
	git submodule update
	$(CL65) -I $(SRCDIR)/mega65-libc/cc65/include -I include -O -o $@ --mapfile $*.map src/fetcherr.c  $(SRCDIR)/mega65-libc/cc65/src/*.c $(SRCDIR)/mega65-libc/cc65/src/*.s src/helper.s

fetchh65.prg:       $(TCPSRCS) src/fetchh65.c src/helper.s include/shared_state.h
	git submodule init
	git submodule update
	$(CL65) -I $(SRCDIR)/mega65-libc/cc65/include -I include -O -o $@ --mapfile $*.map $(TCPSRCS) src/fetchh65.c  $(SRCDIR)/mega65-libc/cc65/src/*.c $(SRCDIR)/mega65-libc/cc65/src/*.s src/helper.s

fetch.prg:       src/fetch.c src/helper.s include/shared_state.h
	$(CL65) -I $(SRCDIR)/mega65-libc/cc65/include -I include -O -o $@ --mapfile $*.map src/fetch.c  $(SRCDIR)/mega65-libc/cc65/src/*.c $(SRCDIR)/mega65-libc/cc65/src/*.s src/helper.s


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


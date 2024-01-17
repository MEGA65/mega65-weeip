.SUFFIXES: .bin .prg
.PRECIOUS:	%.ngd %.ncd %.twx vivado/%.xpr bin/%.bit bin/%.mcs bin/%.M65 bin/%.BIN

USBPORT=	/dev/ttyUSB1

ASSETS=		assets
SRCDIR=		src
BINDIR=		bin
B65DIR=		bin65
EXAMPLEDIR=	$(SRCDIR)/examples
UTILDIR=	$(SRCDIR)/utilities
TESTDIR=	$(SRCDIR)/tests
LIBEXECDIR=	libexec
CC65_PREFIX=	cc65/bin/

CONTENTDIR=	content
SDCARDFILESDIR = sdcard-files

MD2H65=		../mega65-tools/bin/md2h65
#MD2H65=		md2h65
CBMCONVERT = cbmconvert
M65 = m65
M65FTP = mega65_ftp

#CC65=  $(CC65_PREFIX)cc65
#CA65=  $(CC65_PREFIX)ca65 --cpu 4510
#LD65=  $(CC65_PREFIX)ld65 -t none
#CL65=  $(CC65_PREFIX)cl65 --config src/tests/vicii.cfg
#MAPFILE=	--mapfile $*.map

CC65=	llvm-mos/bin/mos-mega65-clang
LD65=	llvm-mos/bin/ld.lld
CL65=	llvm-mos/bin/mos-mega65-clang
MAPFILE=	

MEGA65LIBCDIR= $(SRCDIR)/mega65-libc/cc65
MEGA65LIBCLIB= $(MEGA65LIBCDIR)/libmega65.a
MEGA65LIBCINC= -I $(MEGA65LIBCDIR)/include

SUBDEPENDS=	mega65-tools/bin/md2h65 \
		mega65-tools/bin/asciifont.bin

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
	mkdir -p $(SDCARDFILESDIR)
	cp mega65-tools/bin/asciifont.bin $(SDCARDFILESDIR)/FETCHFNT.M65
	cp fetchm.prg $(SDCARDFILESDIR)/FETCHM.M65
	cp fetcherr.prg $(SDCARDFILESDIR)/FETCHERR.M65
	cp fetchh65.prg $(SDCARDFILESDIR)/FETCHH65.M65
	cp haustierbegriff.prg bbs-client.prg
	if [ -e $(SDCARDFILESDIR)/FETCH.D81 ];then rm $(SDCARDFILESDIR)/FETCH.D81 ;fi
	$(CBMCONVERT) -D8 $(SDCARDFILESDIR)/FETCH.D81 fetch.prg bbs-client.prg

distpush:	dist
	$(M65) -F ; $(M65FTP) -l $(USBPORT) -c "put $(SDCARDFILESDIR)/FETCH.D81" -c "put $(SDCARDFILESDIR)/FETCHM.M65" -c "put $(SDCARDFILESDIR)/FETCHFNT.M65" -c "put $(SDCARDFILESDIR)/FETCHH65.M65" -c "put $(SDCARDFILESDIR)/FETCHERR.M65" -c "quit"

distrun:	distpush
	$(M65) -F -4 -r fetch.prg

distfastrun:	dist
	$(M65) -F ; $(M65FTP) -l $(USBPORT) -c "put $(SDCARDFILESDIR)/FETCHM.M65" -c "put $(SDCARDFILESDIR)/FETCHFNT.M65" -c "put $(SDCARDFILESDIR)/FETCHH65.M65" -c "put $(SDCARDFILESDIR)/FETCHERR.M65" -c "quit"
	$(M65) -F -4 -r fetch.prg

SUBMODULEUPDATE= \
	@if [ -z "$(DO_SMU)" ] || [ "$(DO_SMU)" -eq "1" ] ; then \
	echo "Updating Submodules... (set env-var DO_SMU=0 to turn this behaviour off)" ; \
	git submodule update --init ; \
	fi

$(SUBDEPENDS):
	$(SUBMODULEUPDATE)
	( cd mega65-tools; make bin/md2h65 )

$(MEGA65LIBCLIB):
	$(SUBMODULEUPDATE)
	make -C src/mega65-libc cc65
	make -C src/mega65-libc clean

$(CC65):
	$(SUBMODULEUPDATE)
	( cd cc65 && make -j 8 )

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

fetchm.prg: $(CC65) src/fetchm.c src/helper.s include/shared_state.h $(MEGA65LIBCLIB)
	$(SUBMODULEUPDATE)
	$(CL65) $(MEGA65LIBCINC) -I include -O -o $@ --mapfile $*.map src/fetchm.c $(MEGA65LIBCLIB) src/helper.s

fetcherr.prg: $(CC65) src/fetcherr.c src/helper.s include/shared_state.h $(MEGA65LIBCLIB)
	$(SUBMODULEUPDATE)
	$(CL65) $(MEGA65LIBCINC) -I include -O -o $@ --mapfile $*.map src/fetcherr.c $(MEGA65LIBCLIB) src/helper.s

fetchh65.prg: $(CC65) $(TCPSRCS) src/fetchh65.c src/helper.s include/shared_state.h $(MEGA65LIBCLIB)
	$(SUBMODULEUPDATE)
	$(CL65) $(MEGA65LIBCINC) -I include -O -o $@ --mapfile $*.map $(TCPSRCS) src/fetchh65.c $(MEGA65LIBCLIB) src/helper.s

fetch.prg: $(CC65) src/fetch.c src/helper.s include/shared_state.h $(MEGA65LIBCLIB)
	$(CL65) $(MEGA65LIBCINC) -I include -O -o $@ --mapfile $*.map src/fetch.c $(MEGA65LIBCLIB) src/helper.s

ethtest.prg: $(CC65) $(TCPSRCS) src/ethtest.c src/helper.s $(MEGA65LIBCLIB)
	$(SUBMODULEUPDATE)
	$(CL65) -DENABLE_ICMP=1 $(MEGA65LIBCINC) -I include -O -o ethtest.prg --mapfile $*.map $(TCPSRCS) src/ethtest.c src/helper.s $(MEGA65LIBCLIB)

fetchkc.prg: $(TCPSRCS) src/fetch.c $(MEGA65LIBCLIB)
	$(SUBMODULEUPDATE)
	$(KICKC) -t mega65_c64 -a -I $(SRCDIR)/mega65-libc/kickc/include -I include -L src -L $(SRCDIR)/mega65-libc/kickc/src src/fetch.c

haustierbegriff.prg: $(CC65) $(TCPSRCS) src/haustierbegriff.c $(MEGA65LIBCLIB)
	$(SUBMODULEUPDATE)
	$(CL65) $(MEGA65LIBCINC) -I include -Os -o haustierbegriff.prg --mapfile $*.map $(TCPSRCS) src/haustierbegriff.c $(MEGA65LIBCLIB)


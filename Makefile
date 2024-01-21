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

MEGA65LIBCDIR= $(SRCDIR)/mega65-libc
MEGA65LIBCLIB= $(MEGA65LIBCDIR)/libmega65.a
MEGA65LIBCINC= -I $(MEGA65LIBCDIR)/include

# Uncomment to use CC65
#COMPILER=cc65
#CC65=  $(CC65_PREFIX)cc65
#COMPILERBIN=$(CC65)
#CA65=  $(CC65_PREFIX)ca65 --cpu 4510
#LD65=  $(CC65_PREFIX)ld65 -t none
#CL65=  $(CC65_PREFIX)cl65 --config src/tests/vicii.cfg
#MAPFILE=	--mapfile $*.map
#HELPER=	src/helper-cc65.s
#MEGA65LIBCLIB= $(MEGA65LIBCDIR)/libmega65.a

# Uncomment to use LLVM
COMPILER=llvm
CC65=	llvm-mos/bin/mos-c64-clang -mcpu=mos45gs02
COMPILERBIN=	llvm-mos/bin/mos-c64-clang
LD65=	llvm-mos/bin/ld.lld
CL65=	llvm-mos/bin/mos-c64-clang -DLLVM -mcpu=mos45gs02
MAPFILE=	
HELPER=	src/helper-llvm.s
MEGA65LIBCLIB= $(MEGA65LIBCDIR)/build/src/libmega65libc.a

SUBDEPENDS=	mega65-tools/bin/md2h65 \
		mega65-tools/bin/asciifont.bin

KICKC= ../kickc/bin/kickc.sh

TCPSRCS=	src/arp.c src/checksum.c src/eth.c src/nwk.c src/socket.c src/task.c src/dns.c src/dhcp.c

PRGS= graze.prg grazem.prg grazeerr.prg grazeh65.prg haustierbegriff.prg ethtest.prg
MAPS= $(subst prg,map,$(PRGS))

all:	$(PRGS) pages

clean:
	rm -rf $(CONTENTDIR)
	rm -rf $(SDCARDFILESDIR)
	rm -f $(PRGS)
	rm -f bbs-client.prg
	rm -f $(MAPS)
	rm -f $(MEGA65LIBCLIB)

dist:	all
	mkdir -p $(SDCARDFILESDIR)
	cp mega65-tools/bin/asciifont.bin $(SDCARDFILESDIR)/GRAZEFNT.M65
	cp grazem.prg $(SDCARDFILESDIR)/GRAZEM.M65
	cp grazeerr.prg $(SDCARDFILESDIR)/GRAZEERR.M65
	cp grazeh65.prg $(SDCARDFILESDIR)/GRAZEH65.M65
	cp haustierbegriff.prg bbs-client.prg
	if [ -e $(SDCARDFILESDIR)/GRAZE.D81 ];then rm $(SDCARDFILESDIR)/GRAZE.D81 ;fi
	$(CBMCONVERT) -D8 $(SDCARDFILESDIR)/GRAZE.D81 graze.prg bbs-client.prg

distpush:	dist
	$(M65) -F ; sleep 2 ;  $(M65FTP) -l $(USBPORT) -c "put $(SDCARDFILESDIR)/GRAZE.D81" -c "put $(SDCARDFILESDIR)/GRAZEM.M65" -c "put $(SDCARDFILESDIR)/GRAZEFNT.M65" -c "put $(SDCARDFILESDIR)/GRAZEH65.M65" -c "put $(SDCARDFILESDIR)/GRAZEERR.M65" -c "quit"

distrun:	distpush
	$(M65) -F -4 -r graze.prg

distfastrun:	dist
	$(M65) -F ; $(M65FTP) -l $(USBPORT) -c "put $(SDCARDFILESDIR)/GRAZEM.M65" -c "put $(SDCARDFILESDIR)/GRAZEFNT.M65" -c "put $(SDCARDFILESDIR)/GRAZEH65.M65" -c "put $(SDCARDFILESDIR)/GRAZEERR.M65" -c "quit"
	$(M65) -F -4 -r graze.prg

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
	make -C src/mega65-libc $(COMPILER)

$(CC65_PREFIX)cc65:
	$(SUBMODULEUPDATE)
	( cd cc65 && make -j 8 )

llvm-mos/bin/mos-c64-clang:	llvm-mos
	touch $@ $<

llvm-mos:  llvm-mos-linux.tar.xz
	tar xf llvm-mos-linux.tar.xz

llvm-mos-linux.tar.xz:
	wget https://github.com/llvm-mos/llvm-mos-sdk/releases/latest/download/llvm-mos-linux.tar.xz 


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

grazem.prg: src/grazem.c $(COMPILERBIN) $(HELPER) src/graze_common.c include/shared_state.h $(MEGA65LIBCLIB)
	$(SUBMODULEUPDATE)
	$(CL65) $(MEGA65LIBCINC) -I include -O -o $@ $(MAPFILE) $< $(MEGA65LIBCLIB) $(HELPER)

grazeerr.prg: src/grazeerr.c $(COMPILERBIN) $(HELPER) src/graze_common.c include/shared_state.h $(MEGA65LIBCLIB)
	$(SUBMODULEUPDATE)
	$(CL65) $(MEGA65LIBCINC) -I include -O -o $@ $(MAPFILE) $< $(MEGA65LIBCLIB) $(HELPER)

grazeh65.prg: src/grazeh65.c $(COMPILERBIN) $(TCPSRCS) src/graze_common.c $(HELPER) include/shared_state.h $(MEGA65LIBCLIB)
	$(SUBMODULEUPDATE)
	$(CL65) $(MEGA65LIBCINC) -I include -O -o $@ $(MAPFILE) $(TCPSRCS) $< $(MEGA65LIBCLIB) $(HELPER)

graze.prg: src/graze.c $(COMPILERBIN) $(HELPER) include/shared_state.h $(TCPSRCS) src/graze_common.c $(MEGA65LIBCLIB)
	$(CL65) $(MEGA65LIBCINC) -I include -O -o $@ $(MAPFILE) $< $(TCPSRCS) $(MEGA65LIBCLIB) $(HELPER)

ethtest.prg: src/ethtest.c $(COMPILERBIN) $(TCPSRCS) $(HELPER) $(MEGA65LIBCLIB)
	$(SUBMODULEUPDATE)
	$(CL65) -DENABLE_ICMP=1 $(MEGA65LIBCINC) -I include -O -o ethtest.prg $(MAPFILE) $(TCPSRCS) $< $(HELPER) $(MEGA65LIBCLIB)

fetchkc.prg: $(TCPSRCS) src/fetch.c $(MEGA65LIBCLIB)
	$(SUBMODULEUPDATE)
	$(KICKC) -t mega65_c64 -a -I $(SRCDIR)/mega65-libc/kickc/include -I include -L src -L $(SRCDIR)/mega65-libc/kickc/src src/graze.c

haustierbegriff.prg: src/haustierbegriff.c $(COMPILERBIN) $(TCPSRCS) $(MEGA65LIBCLIB)
	$(SUBMODULEUPDATE)
	$(CL65) $(MEGA65LIBCINC) -I include -Os -o haustierbegriff.prg $(MAPFILE) $(TCPSRCS) $< $(MEGA65LIBCLIB)


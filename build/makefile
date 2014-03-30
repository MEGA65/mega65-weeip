
PROGRAM_NAME = teste
PROGRAM_HEX = $(PROGRAM_NAME).hex
PROGRAM_ASM = $(PROGRAM_NAME).asm
OBJECTS = main.slb task.slb arp.slb eth.slb checksum.slb socket.slb nwk.slb
CC = c:\cpik\bin\cpik073
ASM = c:\cpik\bin\gpasm
DEVICE = p18f97j60
CFLAGS = -I ../include
LDFLAGS =
SRC_PATH = ../src

.phony: all
all: $(PROGRAM_HEX)

$(PROGRAM_HEX): $(OBJECTS)
	$(CC) -p $(DEVICE) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $(PROGRAM_ASM)
	$(CC) -a -p $(DEVICE) -A $(ASM) $(PROGRAM_ASM) -o $(PROGRAM_HEX)

%.slb: $(SRC_PATH)/%.c
	$(CC) -p $(DEVICE) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(PROGRAM_HEX) $(PROGRAM_ASM) $(OBJECTS)

#	$(CC) -p $(DEVICE) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o _.asm
#	@grep -v "IDLOC" _.asm > $(PROGRAM_ASM)
#	$(CC) -a -p $(DEVICE) -A $(ASM) $(PROGRAM_ASM) -o $(PROGRAM_HEX)
#	@rm -f _.asm

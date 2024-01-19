
	.globl mega65_dos_attachd81
	.globl mega65_dos_chdir
	.globl mega65_dos_exechelper
	.globl read_file_from_sdcard
	
set_filename:	
	;; Copy file name
	ldy #0
NameCopyLoop1:
	lda (__rc2),y
	sta $0400,y
	iny
	cmp #0
	bne NameCopyLoop1
	
	;;  Call dos_setname()
	ldy #>$0400
	ldx #<$0400
	lda #$2E     		; dos_setname Hypervisor trap
	STA $D640		; Do hypervisor trap
	NOP			; Wasted instruction slot required following hyper trap instruction
	;; XXX Check for error (carry would be clear)

	rts
	
mega65_dos_exechelper:
	;; char mega65_dos_exechelper(char *image_name);

	jsr set_filename
	
	; close all files to work around hyppo file descriptor leak bug
        lda #$22
        sta $d640
        nop
	
	;; Now copy a little routine into place in $0200 that does the actual loading and jumps into
	;; the program when loaded.
	ldx #$00
lfr1:	lda loadfile_routine,x
	sta $0200,x
	inx
	cpx #$80
	bne lfr1

	;; Call helper routine
	jsr $0200
	
	;; as this is effectively like exec() on unix, it can only return an error
	LDA #$01
	
	RTS

loadfile_routine:
	;; Put dummy routine in at $080d, so that we can tell if it didn't load
	lda #$ee
	sta $080d
	lda #$20
	sta $080e
	lda #$d0
	sta $080f
	lda #$4c
	sta $0810
	lda #$0d
	sta $0811
	lda #$08
	sta $0812

	; Now load the file to $07FF so that the load address bytes make it line up
	;; to $0801
        lda #$36
        ldx #$FF
        ldy #$07
        ldz #$00
        sta $d640
        nop
        ldz #$00

	;; set fake load end address so that CC65 doesn't write over itself
	lda #<$bfff
	sta $2d
	lda #>$bfff
	sta $2e
	
	jmp $080d
	rts
	
mega65_dos_attachd81:
	;; char mega65_dos_attachd81(char *image_name);

	jsr set_filename

	;; Try to attach it
	LDA #$40
	STA $D640
	NOP

	;; return inverted carry flag, so result of 0 = success
	PHP
	PLA
	AND #$01
	EOR #$01
	
	RTS

mega65_dos_chdir:
	;; char mega65_dos_chdir(char *dir_name);

	jsr set_filename
	
	;; Find the file
	LDA #$34
	STA $D640
	NOP
	BCC direntNotFound

	;; Try to change directory to it
	LDA #$0C
	STA $D640
	NOP

direntNotFound:
	
	;; return inverted carry flag, so result of 0 = success
	PHP

	PLA
	AND #$01
	EOR #$01
	
	RTS
	
read_file_from_sdcard:

	;;  read_file_from_sdcard(char *filename,uint32_t load_address);
	;; LLVM-mos calling convention: https://llvm-mos.org/wiki/C_calling_convention
	;; For char * followed by uint32_t, I _think_ it will have the pointer
	;; in rc2-rc3, and the load_address arg in A, X, rc4, rc5
	
	pha
	phx
	
	;; Get pointer to file name
	jsr set_filename

	;; Get Load address into $00ZZYYXX
	pla
	tay
	plx
	lda <__rc4
	taz
	lda #$00

	;; Ask hypervisor to do the load
	LDA #$36
	STA $D640		
	NOP
	;; XXX Check for error (carry would be clear)

	LDZ #$00
	
	RTS


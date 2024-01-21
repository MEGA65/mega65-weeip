
	.altmacro
	
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
	sta $0400,x
	inx
	bne lfr1

	;; Call helper routine
	jsr $0400
	
	;; as this is effectively like exec() on unix, it can only return an error
	LDA #$01
	
	RTS

	// IMPORTANT: The following routine must be fully relocatable, and less than $FF bytes
	// in length
loadfile_routine:

//	lda $d610		
//	beq lf1
//	sta $d610
//	jmp loadfile_routine
//lf1:		
//	inc $d020
//	lda $d610
//	beq lf1
//	sta $d610
	
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

	;; Clear start address accumulator
	lda #$4c
	sta $0100
	lda #$00
	sta $0101
	sta $0102
	
	;; Now find the SYS and entry point
	LDX #$03
find_sys:	
	lda $0800,x
	inx
	cmp #$9e
	bne find_sys

	;; skip any leading spaces
skip_leading_spaces:	
	lda $0800,x
	cmp #$20
	bne spaces_skipped
	inx
	bne skip_leading_spaces

spaces_skipped:
	
process_digit:
	
	lda $0800,x
	cmp #$39
	bcs got_digits
	cmp #$2f
	bcc got_digits

	;; Multiply accumulated value by 10

	;; multiply by 2
	asw $0101
	;; stash in $0103-$0104
	lda $0101
	sta $0103
	lda $0102
	sta $0104
	;; multiply by 4, to get x8
	asw $0101
	asw $0101

	;; Now add the x2 value
	lda $0101
	clc
	adc $0103
	sta $0101
	lda $0102
	adc $0104
	sta $0102

	;; Now add the digit
	lda $0800,x
	and #$0f
	clc
	adc $0101
	sta $0101
	lda $0102
	adc #0
	sta $0102

	inx
	bne process_digit

got_digits:	

	;; Jump to JMP instruction that points to entry point
	jmp $0100

loadfile_routine_end:

.if ((loadfile_routine_end-loadfile_routine)>255)
	.error "load_routine is too long. Max length = 255."
.endif

	
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


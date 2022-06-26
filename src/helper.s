
	.setcpu "65C02"
	.export _mega65_dos_attachd81
	.export _mega65_dos_chdir
	.export _mega65_dos_exechelper
	.export _read_file_from_sdcard
	
	.include "zeropage.inc"
	
.SEGMENT "CODE"

	.p4510
	
_mega65_dos_exechelper:
	;; char mega65_dos_exechelper(char *image_name);

	;; Get pointer to file name
	;; sp here is the ca65 sp ZP variable, not the stack pointer of a 4510
	ldy #1
	.p02
	lda (sp),y
	sta ptr1+1
	sta $0141
	dey
	lda (sp),y
	.p4510
	sta ptr1
	sta $0140
	
	;; Copy file name
	ldy #0
@NameCopyLoop:
	lda (ptr1),y
	sta $0100,y
	iny
	cmp #0
	bne @NameCopyLoop
	
	;;  Call dos_setname()
	ldy #>$0100
	ldx #<$0100
	lda #$2E     		; dos_setname Hypervisor trap
	STA $D640		; Do hypervisor trap
	NOP			; Wasted instruction slot required following hyper trap instruction
	;; XXX Check for error (carry would be clear)

	; close all files to work around hyppo file descriptor leak bug
        lda #$22
        sta $d640
        nop
	
	;; Now copy a little routine into place in $0200 that does the actual loading and jumps into
	;; the program when loaded.
	ldx #$00
@lfr1:	lda loadfile_routine,x
	sta $0200,x
	inx
	cpx #$80
	bne @lfr1

	;; Call helper routine
	jsr $0200
	
	;; as this is effectively like exec() on unix, it can only return an error
	LDA #$01
	LDX #$00
	
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
        ldz     #$00

	;; set fake load end address so that CC65 doesn't write over itself
	lda #<$bfff
	sta $2d
	lda #>$bfff
	sta $2e
	
	jmp $080d
	rts
	
_mega65_dos_attachd81:
	;; char mega65_dos_attachd81(char *image_name);

	;; Get pointer to file name
	;; sp here is the ca65 sp ZP variable, not the stack pointer of a 4510
	ldy #1
	.p02
	lda (sp),y
	sta ptr1+1
	sta $0441
	dey
	lda (sp),y
	.p4510
	sta ptr1
	sta $0440
	
	;; Copy file name
	ldy #0
@NameCopyLoop:
	lda (ptr1),y
	sta $0400,y
	iny
	cmp #0
	bne @NameCopyLoop
	
	;;  Call dos_setname()
	ldy #>$0400
	ldx #<$0400
	lda #$2E     		; dos_setname Hypervisor trap
	STA $D640		; Do hypervisor trap
	NOP			; Wasted instruction slot required following hyper trap instruction
	;; XXX Check for error (carry would be clear)

	;; Try to attach it
	LDA #$40
	STA $D640
	NOP

	;; return inverted carry flag, so result of 0 = success
	PHP
	PLA
	AND #$01
	EOR #$01
	LDX #$00
	
	RTS

_mega65_dos_chdir:
	;; char mega65_dos_chdir(char *dir_name);

	;; Get pointer to file name
	;; sp here is the ca65 sp ZP variable, not the stack pointer of a 4510
	ldy #1
	.p02
	lda (sp),y
	sta ptr1+1
	sta $0441
	dey
	lda (sp),y
	.p4510
	sta ptr1
	sta $0440
	
	;; Copy file name
	ldy #0
@NameCopyLoop:
	lda (ptr1),y
	sta $0400,y
	iny
	cmp #0
	bne @NameCopyLoop
	
	;;  Call dos_setname()
	ldy #>$0400
	ldx #<$0400
	lda #$2E     		; dos_setname Hypervisor trap
	STA $D640		; Do hypervisor trap
	NOP			; Wasted instruction slot required following hyper trap instruction
	;; XXX Check for error (carry would be clear)

	;; Find the file
	LDA #$34
	STA $D640
	NOP
	BCC @direntNotFound

	;; Try to change directory to it
	LDA #$0C
	STA $D640
	NOP

@direntNotFound:
	
	;; return inverted carry flag, so result of 0 = success
	PHP

	PLA
	AND #$01
	EOR #$01
	LDX #$00
	
	RTS
	
_read_file_from_sdcard:

	;;  read_file_from_sdcard(char *filename,uint32_t load_address);

	;; Hypervisor requires copy area to be page aligned, so
	;; we have to copy the name we want to load to somewhere on a page boundary
	;; This is a bit annoying.  I should find out why I made the hypervisor make
	;; such an requirement.  Oh, and it also has to be in the bottom 32KB of memory
	;; (that requirement makes more sense, as it is about ensuring that the
	;; Hypervisor can't be given a pointer that points into its own mapped address space)
	;; As we are not putting any screen at $0400, we can use that
	
	;; Get pointer to file name
	;; sp here is the ca65 sp ZP variable, not the stack pointer of a 4510
	ldy #5
	.p02
	lda (sp),y
	sta ptr1+1
	dey
	lda (sp),y
	.p4510
	sta ptr1

	;; Copy file name
	ldy #0
@NameCopyLoop:
	lda (ptr1),y
	sta $0400,y
	iny
	cmp #0
	bne @NameCopyLoop
	
	;;  Call dos_setname()
	ldy #>$0400
	ldx #<$0400
	lda #$2E     		; dos_setname Hypervisor trap
	STA $D640		; Do hypervisor trap
	NOP			; Wasted instruction slot required following hyper trap instruction
	;; XXX Check for error (carry would be clear)

	;; Get Load address into $00ZZYYXX
	ldy #2
	.p02
	lda (sp),y
	.p4510
	taz
	ldy #0
	.p02
	lda (sp),y
	tax
	iny
	lda (sp),y
	.p4510
	tay

	;; Ask hypervisor to do the load
	LDA #$36
	STA $D640		
	NOP
	;; XXX Check for error (carry would be clear)

	LDZ #$00
	
	RTS


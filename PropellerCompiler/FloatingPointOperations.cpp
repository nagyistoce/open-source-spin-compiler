/*
;
;
;************************************************************************
;*  Floating-Point Operations						*
;************************************************************************
;
; Floating-point routines:
;
;	fp_min		- limit eax => ebx
;	fp_max		- limit eax <= ebx
;	fp_cmp		- compare eax to ebx
;	fp_add		- add ebx into eax
;	fp_sub		- subtract ebx from eax
;	fp_mul		- multiply ebx into eax
;	fp_div		- divide eax into ebx
;	fp_sqr		- compute square root of eax
;
;	fp_float	- convert eax integer to float
;	fp_round	- convert eax float to rounded integer
;	fp_trunc	- convert eax float to truncated integer
;

;
;
; Floating-point minimum (greatest(fp eax, fp ebx) -> fp eax)
; c=1 if overflow
;
fp_min:		call	fp_cmp			;compare fp eax to fb ebx, c=1 if overflow

		jge	@@exit			;if fp eax < fp ebx, return fp ebx
		mov	eax,ebx

@@exit:		ret
;
;
; Floating-point maximum (least(fp eax, fp ebx) -> fp eax)
; c=1 if overflow
;
fp_max:		call	fp_cmp			;compare fp eax to fb ebx, c=1 if overflow

		jle	@@exit			;if fp eax > fp ebx, return fp ebx
		mov	eax,ebx

@@exit:		ret
;
;
; Floating-point comparison (fp eax - fp ebx -> overflow, zero flags)
; c=1 if overflow
;
fp_cmp:		push	eax
		push	ebx

		call	fp_sub			;compare fp eax to fp ebx
		jc	@@exit			;overflow?

		cmp	eax,0			;affect overflow and zero flags
		clc				;c=0

@@exit:		pop	ebx
		pop	eax
		ret
;
;
; Floating-point addition/subtraction (fp eax +/- fp ebx -> fp eax)
; c=1 if overflow
;
fp_sub:		xor	ebx,80000000h		;for subtraction, negate fp ebx

fp_add:		push	ecx
		push	edx
		push	esi
		push	edi

		call	fp_unpack_eax		;unpack floats
		call	fp_unpack_ebx

		shr	dl,1			;perform possible mantissa negations
		jnc	@@apos
		neg	eax
@@apos:		shr	dh,1
		jnc	@@bpos
		neg	ebx
@@bpos:
		cmp	esi,edi			;order unpacked floats by exponent
		jge	@@order
		xchg	eax,ebx
		xchg	esi,edi
@@order:
		mov	ecx,esi			;shift lower mantissa right by exponent difference
		sub	ecx,edi
		cmp	ecx,24
		jbe	@@inrange
		xor	ebx,ebx			;out of range, clear lower mantissa
@@inrange:	sar	ebx,cl

		add	eax,ebx			;add mantissas

		cmp	eax,0			;get sign and absolutize mantissa
		mov	dl,0
		jge	@@rpos
		mov	dl,1
		neg	eax
@@rpos:
		call	fp_pack_eax		;pack float, c=1 if overflow

		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		ret
;
;
; Floating-point multiply (fp eax * fp ebx -> fp eax)
; c=1 if overflow
;
fp_mul:		push	edx
		push	esi
		push	edi

		call	fp_unpack_eax		;unpack floats
		call	fp_unpack_ebx

		xor	dl,dh			;get result sign

		add	esi,edi			;add exponents
		sub	esi,127

		push	edx			;multiply mantissas
		mul	ebx
		shl	edx,3
		mov	eax,edx
		pop	edx

		call	fp_pack_eax		;pack float, c=1 if overflow

		pop	edi
		pop	esi
		pop	edx
		ret
;
;
; Floating-point divide (fp eax / fp ebx -> fp eax)
; c=1 if overflow
;
fp_div:		push	edx
		push	esi
		push	edi

		call	fp_unpack_eax		;unpack floats
		call	fp_unpack_ebx

		or	ebx,ebx			;check for divide by 0
		stc
		jz	@@exit

		xor	dl,dh			;get result sign

		sub	esi,edi			;subtract exponents
		add	esi,127

		xor	edi,edi			;divide mantissas
		mov	dh,30
@@div:		cmp	eax,ebx
		jb	@@not
		sub	eax,ebx
@@not:		cmc
		rcl	edi,1
		shl	eax,1
		dec	dh
		jnz	@@div
		mov	eax,edi

		call	fp_pack_eax		;pack float, c=1 if overflow

@@exit:		pop	edi
		pop	esi
		pop	edx
		ret
;
;
; Floating-point square root (sqr(fp eax) -> fp eax)
; c=1 if imaginary
;
fp_sqr:		push	edx
		push	esi

		or	eax,eax			;if mantissa 0, result 0, c=0
		jz	@@exit
		stc				;if negative, c=1
		js	@@exit

		call	fp_unpack_eax		;unpack float

		sub	esi,127			;determine root exponent
		sar	esi,1
		rcl	dh,1			;remember exponent even/odd
		add	esi,127

		test	dh,1			;if exponent even, bit30-justify mantissa
		jnz	@@just
		shl	eax,1
@@just:
		push	ebx
		push	ecx
		push	edx
		push	esi

		mov	ebx,eax			;successively approximate root
		xor	ecx,ecx
		mov	esi,80000000h
@@next:		or	ecx,esi
		mov	eax,ecx
		mul	ecx
		cmp	ebx,edx
		jae	@@keep
		xor	ecx,esi
@@keep:		shr	esi,1
		jnc	@@next
		mov	eax,ecx
		shr	eax,1

		pop	esi
		pop	edx
		pop	ecx
		pop	ebx

		test	dh,1			;bit29-justify mantissa
		jnz	@@just2
		shr	eax,1
@@just2:
		call	fp_pack_eax		;pack float, c=0

@@exit:		pop	esi
		pop	edx
		ret
;
;
; Convert integer to floating-point (int eax -> fp eax)
;
fp_float:	push	edx
		push	esi

		mov	edx,eax			;get sign
		shr	edx,31

		or	eax,eax			;if mantissa 0, result 0
		jz	@@exit

		jns	@@pos			;absolutize mantissa
		neg	eax
@@pos:
		mov	esi,32+127		;determine exponent and mantissa
@@exp:		dec	esi
		shl	eax,1
		jnc	@@exp
		rcr	eax,1			;replace leading 1
		shr	eax,2			;bit29-justify mantissa

		call	fp_pack_eax		;pack float

@@exit:		pop	esi
		pop	edx
		ret
;
;
; Convert float to rounded/truncated integer (fp eax -> int eax)
; c=1 if overflow
;
fp_round:	stc
		jmp	fp_rt

fp_trunc:	clc

fp_rt:		push	ecx
		push	edx
		push	esi

		rcl	dh,1			;get round flag in dh.0

		call	fp_unpack_eax		;unpack float
		shl	eax,2			;bit31-justify mantissa

		mov	ecx,30+127		;if exponent > 30, overflow, c=1
		sub	ecx,esi
		stc
		jl	@@exit
		cmp	esi,-1+127		;if exponent 0..30, integer
		jg	@@integer
		mov	eax,0			;if exponent < -1, zero
		jl	@@done
		shr	dh,1			;exponent -1, 1/2 rounds to 1
		rcl	eax,1
		jmp	@@neg

@@integer:	shr	eax,cl			;in range, round and justify
		shr	dh,1
		adc	eax,0
		shr	eax,1

@@neg:		shr	dl,1			;negative?
		jnc	@@pos
		neg	eax
@@pos:
@@done:		clc				;done, c=0

@@exit:		pop	esi			;c=1 if overflow
		pop	edx
		pop	ecx
		ret
;
;
; Unpack eax
;
; dl.0=sign, esi=exponent, eax = mantissa (bit29-justified)
; if mantissa 0, value 0
;
fp_unpack_eax:	shl	eax,1			;get sign a
		rcl	dl,1

		mov	esi,eax			;get exponent a
		shr	esi,24

		shl	eax,8			;get mantissa a
		or	esi,esi			;if exponent not 0, add msb
		jnz	@@nz

		or	eax,eax			;if mantissa 0, done
		jz	@@z

		inc	esi			;adjust exp and mantissa
@@adj:		dec	esi
		shl	eax,1
		jnc	@@adj

@@nz:		stc				;install/replace leading 1
		rcr	eax,1
		shr	eax,2			;bit29-justify mantissa
@@z:
		ret
;
;
; Unpack ebx
;
; dh.0=sign, edi=exponent, ebx = mantissa (bit29-justified)
; if mantissa 0, value 0
;
fp_unpack_ebx:	shl	ebx,1			;get sign b
		rcl	dh,1

		mov	edi,ebx			;get exponent b
		shr	edi,24

		shl	ebx,8			;get mantissa b
		or	edi,edi			;if exponent not 0, add msb
		jnz	@@nz

		or	ebx,ebx			;if mantissa 0, done
		jz	@@z

		inc	edi			;adjust exp and mantissa
@@adj:		dec	edi
		shl	ebx,1
		jnc	@@adj

@@nz:		stc				;install/replace leading 1
		rcr	ebx,1
		shr	ebx,2			;bit29-justify mantissa
@@z:
		ret
;
;
; Pack eax
;
; dl.0=sign, esi=exponent, eax = mantissa (bit29-justified)
; c=1 if overflow
;
fp_pack_eax:	or	eax,eax			;if mantissa 0, result 0, c=0
		jz	@@exit

		add	esi,3			;adjust exponent by mantissa
@@exp:		dec	esi
		shl	eax,1
		jnc	@@exp

		add	eax,100h		;round to nearest mantissa lsb
		adc	esi,0

		cmp	esi,0			;if exponent > 0, pack result
		jg	@@pack

		stc				;exponent =< 0, unnormalized mantissa
		rcr	eax,1			;replace leading 1
@@ushr:		or	esi,esi			;shift unnormalized mantissa right
		jz	@@pack			;...and inc exponent until 0
		shr	eax,1
		inc	esi
		jmp	@@ushr

@@pack:		shr	ah,1			;pack result
		shr	dl,1
		rcl	ah,1
		mov	edx,esi
		mov	al,dl
		ror	eax,9

		cmp	esi,0FFh		;c=1 if overflow
		cmc

@@exit:		ret
*/
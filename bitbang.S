;  bitbang.s - serial output using bit-banging
;
;  Copyright (c) 2009 by Matthias Reichl <hias@horus.com>
;
;  This program is free software; you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation; either version 2 of the License, or
;  (at your option) any later version.
;
;  This program is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;  GNU General Public License for more details.
;
;  You should have received a copy of the GNU General Public License
;  along with this program; if not, write to the Free Software
;  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <avr/io.h>

.global USART_Transmit_Byte

.global DelayAtariX

.func USART_Transmit_Byte

USART_Transmit_Byte:

; register usage:
; r27 = transmit bit counter
; r26 = transmit byte
; r25 = bit-delay value

; transmit low start bit
	cbi _SFR_IO_ADDR(PORTD), PIND1	; 2

; update tx_checksum
	lds r25, tx_checksum		; 2
	ldi r26, 0			; 1
; tx_checksum = tx_checksum + byte
	add r25, r24			; 1
; tx_checksum = tx_checksum + carry
	adc r25, r26			; 1
	sts tx_checksum, r25		; 2
					; 7 cycles since start bit

; r27 is used as a bit counter
; note: it's initialized to 9, as the stop-bit will be transmitted
; by the main bit loop
	ldi r27,9			; 1

; move byte from input (r24) to r26
	mov r26, r24			; 1

; read current baudrate register
	;in r25, _SFR_IO_ADDR(UBRRL)	; 1
	lds r25, UBRR0L			; 2

; calculate delay value:
; baudrate is clock_frequency / 16 / (UBRR+1) * 2
; DelayAtariX delays for X*8 cycles, so we have to multiply
; the delay by 2. The remaining 16 cycles are spent in "bitlp"
;
; Note: total bit duration time was changed from 16+bit_delay*8
; to 17+bit_delay*8 to compensate for the slightly slower
; clock speeds in PAL Ataris.

	;we have already the double value due to U2X!
	;lsl r25				; 1

; stretch start-bit by 5 atari clock cycles to compensate for
; Pokey's too-late data sampling

	ldi r24, 4			; 1
	add r24, r25			; 1
					; 13 cycles since start bit

	rcall DelayAtariX		; 8*(r24+4) = 32 + 8*r24
					; 45 cycles since start bit

; next bit transition will be in 6 Atmel cycles after this
; delay, target is 17+40+bit_delay*8 = 57 + bit_delay*8
; so waste 57-45-6 = 6 more cycles

	ldi r24,2			; 1
bitdel1:
	dec r24				; 1
	brne bitdel1			; 1/2

; bitlp executed 9 times, 8 data bits + 1 stop bit
; carry is set to 1 so that the stopbit will be 1
bitlp:	sec				; 1
	ror r26				; 1

	brcc bit0			; 1/2

; transmit high bit
					; 3 cycles since bitlp
	sbi _SFR_IO_ADDR(PORTD), PIND1 	; 2
	rjmp bitdel			; 2

; transmit low bit

bit0:					; 4 cycles since bitlp
	cbi _SFR_IO_ADDR(PORTD), PIND1 	; 2
	nop				; 1

bitdel:					; 7 cycles per bit so far

	mov r24, r25			; 1
	rcall DelayAtariX		; 8*r24

; delay for 6 Atmel cycles so that the total bitlp takes 113
; cycles at pokey divisor 0. This compensates for the slightly slower
; crystal on PAL Ataris.

	ldi r24, 2			; 1
bitdel2:
	dec r24				; 1
	brne bitdel2			; 2/1
					; this delay: 1 + (2*3 - 1) = 6 cycles

	dec r27				; 1
	brne bitlp			; 1/2
	nop				; 1

					; total: 17 + r26*8 cycles per data bit
	ret

.endfunc

.func DelayAtariX

; delay for r24 Atari clock cycles (8*r24 Atmel cycles)
; note: r24 must be >=2
DelayAtariX:				; 3 cycles for "rcall DelayAtariX"
		dec r24			; 1
delata1:	nop			; 1
		nop			; 1
		nop			; 1
		nop			; 1
		nop			; 1
		dec r24			; 1
		brne delata1		; 1/2
		nop			; 1
		ret			; 4

.endfunc

; TurBoss 2024
;   Agon Light UART 1 helper

	assume  adl=1

	section .text

	public		_UART1_INIT
	public		_UART1_SEND
	public		_UART1_READ

	; UART1
	PORTC_DRVAL_DEF       EQU    0ffh			;The default value for Port C data register (set for Mode 2).
	PORTC_DDRVAL_DEF      EQU    0ffh			;The default value for Port C data direction register (set for Mode 2).
	PORTC_ALT0VAL_DEF     EQU    0ffh			;The default value for Port C alternate register-0 (clear interrupts).
	PORTC_ALT1VAL_DEF     EQU    000h			;The default value for Port C alternate register-1 (set for Mode 2).
	PORTC_ALT2VAL_DEF     EQU    000h			;The default value for Port C alternate register-2 (set for Mode 2).


	PC_DR		EQU	09Eh
	PC_DDR		EQU	09Fh
	PC_ALT1		EQU	0A0h
	PC_ALT2		EQU	0A1h

	;* UART1 Registers

	UART1_RBR	EQU 0D0h
	UART1_THR	EQU 0D0h
	UART1_BRG_L	EQU 0D0h
	UART1_IER	EQU 0D1h
	UART1_BRG_H	EQU 0D1h
	UART1_IIR	EQU 0D2h
	UART1_FCTL	EQU 0D2h
	UART1_LCTL	EQU 0D3h
	UART1_MCTL	EQU 0D4h
	UART1_LSR	EQU 0D5h
	UART1_MSR	EQU 0D6h
	UART1_SPR	EQU 0D7h

	; baudrate divisors 31250 UART 1 MIDI
	; 18432000 / (16*31250) = 36,864
	BRD_LOW_1	EQU	24h
	BRD_HIGH_1	EQU	16h

_UART1_INIT:
	; all pins to GPIO mode 2, high impedance input
	LD		A,				PORTC_DRVAL_DEF
	OUT0	(PC_DR),		A
	LD		A,				PORTC_DDRVAL_DEF
	OUT0	(PC_DDR),		A
	LD		A,				PORTC_ALT1VAL_DEF
	OUT0	(PC_ALT1),		A
	LD		A,				PORTC_ALT2VAL_DEF
	OUT0	(PC_ALT2),		A
	; initialize for correct operation
	; pin 0 and 1 to alternate function
	; set pin 3 (CTS) to high-impedance input
	IN0		A,				(PC_DDR)
	OR		00001011b					; set pin 0,1,3
	OUT0	(PC_DDR),		A
	IN0		A,				(PC_ALT1)
	AND		11110100b					; reset pin 0,1,3
	OUT0	(PC_ALT1),		A
	IN0		A,				(PC_ALT2)
	AND		11110111b					; reset pin 3
	OR		00000011b					; set pin 0,1
	OUT0	(PC_ALT2),		A
	IN0		A,				(UART1_LCTL)
	OR		10000000b 					; set UART_LCTL_DLAB
	OUT0	(UART1_LCTL),	A
	LD		A,				BRD_LOW_1 	; Load divisor low
	OUT		(UART1_BRG_L),	A
	LD		A,				BRD_HIGH_1 	; Load divisor high
	OUT0	(UART1_BRG_H),	A
	IN0		A,				(UART1_LCTL)
	AND		01111111b					; reset UART_LCTL_DLAB
	OUT0	(UART1_LCTL),	A
	LD		A,				000h		; reset modem control register
	OUT0	(UART1_MCTL),	A
	LD		A,				007h		; enable and clear hardware fifo's
	OUT0	(UART1_FCTL),	A
	LD		A,				000h		; no interrupts
	OUT0	(UART1_IER),	A
	IN0		a,				(UART1_LCTL)
	OR		00000011b					; 8 databits, 1 stopbit
	AND		11110111b					; no parity
	OUT0	(UART1_LCTL),	A
	RET

_UART1_SEND:
    PUSH	AF
uart1_available:
	IN0		A,				(UART1_LSR)
	AND		01000000b 					; 040h = Transmit holding register/FIFO and transmit shift register are empty
	JR		Z,				uart1_available
	POP		AF
	OUT0	(UART1_THR),	A
	; RST.LIL 10h
	RET


_UART1_READ:
	; Check if the receive buffer is full
	IN0 B, (UART1_LSR)
	BIT 0, B
	RET Z  ; Return immediately if no character is available
	; Read the character from the receive buffer
	IN0 A, (UART1_RBR)
	RET

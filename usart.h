//*****************************************************************************
// usart.h
// Bob!k & Raster, C.P.U., 2008
// Ehanced by kbr, 2014
//*****************************************************************************

#include "avrlibdefs.h"                 // global AVRLIB defines
#include "avrlibtypes.h"                // global AVRLIB types definitions

#if defined(__AVR_ATmega328__) || defined(__AVR_ATmega168__)
	#define UCSRA   UCSR0A
	#define UCSRB   UCSR0B
	#define UCSRC   UCSR0C
	#define UDRE    UDRE0
	#define RXEN    RXEN0
	#define TXEN    TXEN0
	#define URSEL   0		// does not exist
	#define UCSZ0   UCSZ00
	#define UCSZ1   UCSZ01
	#define U2X     U2X0
	#define RXC     RXC0
	#define TXC     TXC0
	#define UDR     UDR0
	#define FE      FE0
	#define DOR     DOR0
#endif

#define send_ACK()	USART_Transmit_Byte('A')
#define send_NACK()	USART_Transmit_Byte('N')
#define send_CMPL()	USART_Transmit_Byte('C')
#define send_ERR()	USART_Transmit_Byte('E')

#define USART_Get_atari_sector_buffer_and_check_and_send_ACK_or_NACK(len)	USART_Get_buffer_and_check_and_send_ACK_or_NACK(atari_sector_buffer,len)

#define wait_cmd_HL() { while ( (inb(CMD_PORT) & (1<<CMD_PIN)) ); }
#define wait_cmd_LH() { while ( !(inb(CMD_PORT) & (1<<CMD_PIN)) ); }

#define CMD_STATE_H             (1<<CMD_PIN)
#define CMD_STATE_L             0
#define get_cmd_H()             ( inb(CMD_PORT) & (1<<CMD_PIN) )
#define get_cmd_L()             ( !(inb(CMD_PORT) & (1<<CMD_PIN)) )

unsigned char get_checksum(unsigned char* buffer, u16 len);

//prototypes
void USART_Init( u16 value );
void USART_Flush();
void USART_Transmit_Byte( unsigned char data );
unsigned char USART_Receive_Byte( void );
void USART_Send_Buffer(unsigned char *buff, u16 len);
u08 USART_Get_Buffer_And_Check(unsigned char *buff, u16 len, u08 cmd_state);
u08 USART_Get_buffer_and_check_and_send_ACK_or_NACK(unsigned char *buff, u16 len);
//void USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(unsigned short len);
void USART_Send_buffer_and_check_sum(unsigned char *buf, unsigned short len, unsigned char status);

#define USART_Send_ERR_and_atari_sector_buffer_and_check_sum(len) USART_Send_buffer_and_check_sum(atari_sector_buffer, len, 1)
#define USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(len) USART_Send_buffer_and_check_sum(atari_sector_buffer, len, 0)
#define USART_Send_cmpl_and_buffer_and_check_sum(buf, len) USART_Send_buffer_and_check_sum(buf, len, 0)

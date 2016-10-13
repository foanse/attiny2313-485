#include <avr/io.h>
#include <avr/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

#define F_CPU 8000000L
#define MAX 20
#define MEM 195
#define sleep _delay_us(1)

unsigned char PLAN[32];
unsigned char BLOCK[32];
unsigned char number;
unsigned char DEV1,DEV2,ER1,ER2;
volatile unsigned short COUNT_COMAND;

volatile unsigned short CRC;
volatile unsigned char status;
volatile unsigned char BUF[MAX],COUNT;
ISR(TIMER1_COMPA_vect)
{
	PORTD&=~(0x40);
	TCCR1B=0x00;
}
ISR(USART_RX_vect)
{
//	if(status&0x80) return;
	BUF[COUNT]=UDR;
	if(UCSRA&0x1C)
		status|=0x10;
	if(status&0x20)
		status|=0x10;	
	if(COUNT>=MAX)
		status|=0x10;
	else
		COUNT++;
	TCNT0=0x00;
	TCCR0B=0x05;
}
ISR(TIMER0_COMPA_vect)
{
	if((status&0x10)==0){
		status|=0x80;	
	}
	else
	{
		status=0x00;
		COUNT=0;
	}		
	TCCR0B=0x00;
}	
ISR(TIMER0_COMPB_vect)
{
	status|=0x20;
}

void USART_Init( unsigned char i )
{
	unsigned int baudrate;
	switch (i){
		default: 
					baudrate=103;	OCR0A=0x1C;	OCR0B=0x0C;
	}
	UBRRH = (unsigned char) (baudrate>>8);
	UBRRL = (unsigned char) baudrate;
	UCSRA = (1<<U2X);
	UCSRB = (1<<RXEN)|(1<<TXEN)|(1<<RXCIE);
	UCSRC = (0<<USBS)|(1<<UCSZ0)|(1<<UCSZ1);
} 
void sendchar(unsigned char data){
	if(number!=BUF[0]) return;
    unsigned char i;
	CRC ^= data;
	for (i = 8; i != 0; i--) {
		if ((CRC & 0x0001) != 0) {
			CRC >>= 1;
			CRC ^= 0xA001;
		}
		else
			CRC >>= 1;
	}
	if(!(PORTD&0x04)){
		PORTD|=0x04;
		_delay_ms(15);
	}
	while ( !(UCSRA & (1<<UDRE)) );
	UDR = data;			        
}
void sendcrc(){
	if(number!=BUF[0]) return;
	while ( !(UCSRA & (1<<UDRE)) );
	UDR = (unsigned char)(CRC>>8);			        
	while ( !(UCSRA & (1<<UDRE)) );
	UDR = (unsigned char)CRC;			        
	while ( !(UCSRA & (1<<UDRE)) );
	_delay_ms(15);
	PORTD&=~(0x04);
	CRC=0xFFFF;
}
unsigned char CRCin(unsigned char count)
{
	unsigned short crc = 0xFFFF;
    unsigned char pos,i;
    for (pos = 0; pos < count; pos++) {
		crc ^= (unsigned char)BUF[pos];
		for (i = 8; i != 0; i--) {
			if ((crc & 0x0001) != 0) {
				crc >>= 1;
				crc ^= 0xA001;
			}
		else
			crc >>= 1;
		}
    }
	if((((unsigned char)(crc>>8))==BUF[count])&&(((unsigned char)crc)==BUF[count+1])) 
		return 1;
	else
		return 0;
}
unsigned char read_registr_param(unsigned char address)
{
	if(address<32)
		return PLAN[address];
	if(address<64)
		return BLOCK[address-32];
	if(address<192)
		return eeprom_read_byte(address-64);
	if(address==192)
		return ((PORTD&0x20)>>1)|((PORTD&0x10)>>4);
	if(address==193)
		return (ER1&0xC0)|(DEV1&0x3F);
	if(address==194)
		return (ER2&0xC0)|(DEV2&0x3F);
	if(address==195)
		return 0;
	if(address==196)
		return 0;		
}
void write_registr_param(unsigned char address, unsigned char data)
{
	if(address<32){
		PLAN[address]=data&BLOCK[address];
		return;
	}		
	if(address<64){
		BLOCK[address-32]=data;
		PLAN[address-32]&=data;
		return;
	}
	if(address<192){
		eeprom_write_byte(address-64,data);
		return;
	}		
	if(address==192){
		if(data&0x01) PORTD|=0x10; 
		if(data&0x02) PORTD&=~(0x10);
		if(data&0x04) PORTD|=0x20;
		if(data&0x08) PORTD&=~(0x20);
		return;
	}
	if(address==193){
		DEV1=data;
		ER1=0;
		DDRB|=0x07;	
		return;
	}		
	if(address==194){
		DEV2=data;
		ER2=0;
		DDRB|=0x70;	
		return;
	}		
	if(address==195){
		eeprom_write_byte(1,data);
		number=data;
		return;
	}		
/*	if(address==196){
		USART_Init(data);
		return;
	}*/		
}

void swit(){
	sendchar(number);
	unsigned short B,E;
	unsigned char c,d;
	switch(BUF[1]){
	case 0x01:
			if(COUNT!=8) goto er;
			B=(BUF[2]<<8)|BUF[3];
			if(B>(195*8-1)) goto er;
			sendchar(0x01);
			sendchar(0x01);
			d=B>>3;
			B&=0x0007;
			c=read_registr_param(d);
			if((c&(1<<B))>0)
				sendchar(0x01);
			else
				sendchar(0x00);
			goto re;
//				case 0x02:read_bit_input();break;
	case 0x03:
//	case 0x04:
			if(COUNT!=8) goto er;
			if((BUF[3]+BUF[5])>MEM) goto er;			
			sendchar(0x03);
			sendchar(BUF[5]);
			for(d=BUF[3];d<(BUF[3]+BUF[5]);d++){	
				sendchar(0);
				sendchar(read_registr_param(d));
			}								
			goto re;
	case 0x05:
			if(COUNT!=8) goto er;
			B=(BUF[2]<<8)|BUF[3];
			if(B>(MEM*8-1))goto er;
			d=(B>>3);
			B&=0x0007;
			c=read_registr_param(d);
			if(BUF[4]==0xFF) 
				c|=(1<<B);
			if(BUF[5]==0xFF)
				c&=~(1<<B);
			write_registr_param(d,c);
			sendchar(0x05);
			sendchar(BUF[2]);
			sendchar(BUF[3]);
			sendchar(0x00);
			sendchar(0x01);
			goto re;
	case 0x06:		
			if(COUNT!=8) goto er;
			if((BUF[3]+BUF[5])>MEM) goto er;			
			sendchar(0x06);
			sendchar(0);sendchar(BUF[3]);
			sendchar(0);sendchar(BUF[5]);
			write_registr_param(BUF[3],BUF[5]);
			goto re;
	case 0x0B:
			if(COUNT!=4) goto er;
			sendchar(0x0B);
			sendchar((unsigned char)(COUNT_COMAND>>8));
			sendchar((unsigned char)COUNT_COMAND);
			goto re;
//				case 0x0F:write_bits_output();break;
//				case 0x10:write_registrs_param();break;
	case 0x11:
			if(COUNT!=4) goto er;
			sendchar(0x11);sendchar(3);sendchar(0x23);sendchar(0x01);sendchar(0xFF);
			goto re;
	default:
			COUNT_COMAND--;
			goto er;
	}
er:
	sendchar(BUF[1]|0x80);
	sendchar(0x01);
re:	
	sendcrc();
}


void main(void)
{
	unsigned char i1,i2,j,e1,e2;
/*	if(eeprom_read_byte(0)==0xff){
		eeprom_write_byte(0,0x00);
		eeprom_write_byte(2,0x00);
		eeprom_write_byte(3,0x00);
		eeprom_write_byte(4,0x00);
		for(i=8;i<80;i++)
			eeprom_write_byte(i,0x00);
	}
*/	USART_Init(eeprom_read_byte(0));
	number=eeprom_read_byte(1);
	DEV1=eeprom_read_byte(2);
	DEV2=eeprom_read_byte(3);
	ER1=0;
	ER2=0;
	DDRB= 0x77;
	DDRD= 0x74;
	PORTD=eeprom_read_byte(4)&0x30;
	for(COUNT=0;COUNT<32;COUNT++){
		BLOCK[COUNT]=eeprom_read_byte(40+COUNT);
		PLAN[COUNT]=eeprom_read_byte(8+COUNT)&BLOCK[COUNT];
	}
	COUNT=0;
	status=0;
	COUNT_COMAND=0;
	CRC=0xFFFF;
	TCCR1A=0x00;
	OCR1AH=0x08;
	OCR1AL=0x00;
	TCCR0A=0x02;
	TCCR0B=0x00;
	TIMSK=(1<<OCIE0A)|(1<<OCIE0B)|(1<<OCIE1A);
	sei();
	while(1){
		if(status&0x80){
			if((number==BUF[0]|BUF[0]==0x00)){
				if(CRCin(COUNT-2)){
					PORTD|=0x40;
					TCCR1B=0x0D;
					COUNT_COMAND++;
					swit();
					}
			}	
			status=0;
			COUNT=0;
		}	
		if(ER1>0xC0) DDRB&=~(0x07);
		if(ER2>0xC0) DDRB&=~(0x70);
		PORTB&=~(0x11);
		sleep;
		if(PLAN[i1]&(1<<j)){
			if(!(PINB&0x08)) e1++;
			PORTB|=0x02;
		}else{
			if(PINB&0x08) e1++;
			PORTB&=~(0x02);
		}				
		if(PLAN[i2]&(1<<j)){
			if(!(PINB&0x80)) e2++;
			PORTB|=0x20;
		}else{
			if(PINB&0x80) e2++;
			PORTB&=~(0x20);
		}
		sleep;
		PORTB|=0x11;
		sleep;
		j++;
		if(j>7){j=0;i1++;i2++;}
		if(i1>=DEV1){
			i1=0;
			if(e1==0){
				PORTB&=~(0x04);
				ER1=0;
			}
			ER1++;
			e1=0;
		}
		if(i2>=(DEV1+DEV2)){
			i2=DEV1;
			if(e2==0){
				PORTB&=~(0x40);
				ER2=0;
			}
			ER2++;
			e2=0;
		}
		sleep;
		PORTB|=0x44;
	}
}

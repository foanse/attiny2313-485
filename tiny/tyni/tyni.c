#include <avr/io.h>
#include <avr/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

#define F_CPU 8000000L
#define MAX 20
#define SPI 24
//#define MEM 223
//#define MEMb MEM*8-1
#define sleep _delay_ms(10)

volatile unsigned char PLAN[SPI];
volatile unsigned char BLOCK[SPI];
volatile unsigned char number, DEV1,DEV2,ER1,ER2;
volatile unsigned short COUNT_COMAND;
volatile unsigned short CRC;
volatile unsigned char status;
volatile unsigned char BUF[MAX],COUNT;

void crc(unsigned char data){
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
}
void sendchar(unsigned char data){
	crc(data);
	while ( !(UCSRA & (1<<UDRE)) );
	UDR = data;			        
}
void sendcrc(){
	while ( !(UCSRA & (1<<UDRE)) );
	UDR = (unsigned char)(CRC>>8);			        
	while ( !(UCSRA & (1<<UDRE)) );
	UCSRA|=(1<<TXC);
	UDR = (unsigned char)CRC;			        
	while ( !(UCSRA & (1<<TXC)) );
	PORTD&=~(0x04);
}
unsigned char read_registr_param(unsigned char address)
{
	if(address==8)
		return ((PORTD&0x20)>>1)|((PORTD&0x10)>>4);
	if(address<30)
		return 0;
	if(address==30)
		return (DEV1&0x3F);
	if(address==31)
		return (DEV2&0x3F);
	if(address<64)
		return PLAN[address-32];
	if(address<96)
		return BLOCK[address-64];
	if(address<224)
		return eeprom_read_byte(address-96);
	return 0;
}
void write_registr_param(unsigned char address, unsigned char data)
{
	if(address==0){
		eeprom_write_byte(1,data);
		number=data;
		return;
	}
	if(address==8){
		if(data&0x01) PORTD|=0x10; 
		if(data&0x02) PORTD&=~(0x10);
		if(data&0x04) PORTD|=0x20;
		if(data&0x08) PORTD&=~(0x20);
		return;
	}
	if(address<30)return;
	if(address==30){
		DEV1=data;
		ER1=0;
		return;
	}
	if(address==31){
		DEV2=data;
		ER2=0;
		return;
	}
	if(address<64){
		PLAN[address-32]=data&BLOCK[address-32];
		return;
	}		
	if(address<96){
		BLOCK[address-64]=data;
		PLAN[address-64]&=data;
		return;
	}
	if(address<224){
		eeprom_write_byte(address-96,data);
		return;
	}		
}
void swit(){
	CRC=0xFFFF;
	sendchar(number);
	unsigned short B,E;
	unsigned char c,d;
	switch(BUF[1]){
	case 0x01:
			if(COUNT!=8) goto er;
			B=(BUF[2]<<8)|BUF[3];
			sendchar(0x01);
			sendchar(0x01);
			d=B>>3;
			B&=0x0007;
			c=read_registr_param(d);
			if(c&(1<<B))
				sendchar(0x01);
			else
				sendchar(0x00);
			goto re;
	case 0x03:
	case 0x04:
			if(COUNT!=8) goto er;
			sendchar(BUF[1]);
			sendchar(BUF[5]);
			for(d=BUF[3];d<(BUF[3]+BUF[5]);d++){	
				sendchar(0);
				sendchar(read_registr_param(d));
			}								
			goto re;
	case 0x05:
			if(COUNT!=8) goto er;
			B=(BUF[2]<<8)|BUF[3];
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
	case 0x11:
			if(COUNT!=4) goto er;
			sendchar(0x11);sendchar(3);sendchar(0x23);sendchar(0x02);sendchar(0xFF);
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
ISR(TIMER0_COMPB_vect)
{
	TCCR0B=0x00;
	if(status&0x10) goto end;
	if((number!=BUF[0])&&(BUF[0]!=0x00)) goto end;
	if((((unsigned char)(CRC>>8))!=BUF[COUNT-2])||(((unsigned char)CRC)!=BUF[COUNT-1])) goto end;
	if(number==BUF[0])PORTD|=0x04;
	PORTD|=0x40;
	TCCR1B=0x0D;
	COUNT_COMAND++;
	swit();
	end:
	CRC=0xFFFF;
	status=0x00;
	COUNT=0;
}	
ISR(TIMER0_COMPA_vect)
{
	status|=0x20;
}
ISR(USART_RX_vect)
{
	unsigned char S=UCSRA;
	BUF[COUNT]=UDR;
	if((S&0x1C)||(status&0x20)||(COUNT>=MAX)){
		CRC=0xFFFF;
		COUNT=0;
		status|=0x10;
		return;
	}
	if(COUNT>1) crc(BUF[COUNT-2]);
	COUNT++;
	TCNT0=0x00;
	TCCR0B=0x05;
}
ISR(TIMER1_COMPA_vect)
{
	PORTD&=~(0x40);
	TCCR1B=0x00;
}
void USART_Init(void)
{
	TIMSK=0;
	//UART
	UBRRH = 0;
	UBRRL = 103;
	UCSRA = (1<<U2X);
	UCSRB = (1<<RXEN)|(1<<TXEN)|(1<<RXCIE);
	UCSRC = (0<<USBS)|(1<<UCSZ0)|(1<<UCSZ1);

	//timer0 
	TCCR0A=0x00;
	TCCR0B=0x00;
	OCR0A=0x0C;
	OCR0B=0x1C;
	TIMSK|=(1<<OCIE0A)|(1<<OCIE0B);
	
	//timer1
	TCCR1A=0x00;
	OCR1AH=0x02;
	OCR1AL=0x00;
	TIMSK|=(1<<OCIE1A);
} 

void main(void)
{
unsigned char RELE;
unsigned char i1,i2,j,e1,e2;
	USART_Init();
	number=eeprom_read_byte(1);
	DEV1=eeprom_read_byte(2);
	DEV2=eeprom_read_byte(3);
	ER1=0;
	ER2=0;
	DDRB= 0x77;
	DDRD= 0x74;
	RELE=eeprom_read_byte(4);
	PORTD=RELE&0x30;
	for(COUNT=0;COUNT<SPI;COUNT++){
		BLOCK[COUNT]=eeprom_read_byte(40+COUNT);
		PLAN[COUNT]=eeprom_read_byte(8+COUNT)&BLOCK[COUNT];
	}
	COUNT=0;
	status=0;
	COUNT_COMAND=0;
	CRC=0xFFFF;
	sei();
	while(1){
		if((ER1==0xFF)&&(RELE&0x01))PORTD&=~(0x10);		
		if((ER2==0xFF)&&(RELE&0x02))PORTD&=~(0x20);
		PORTB&=~(0x33);sleep;
		if(PLAN[i1]&(1<<j)){PORTB|=0x02;}else{if(PINB&0x08) e1++;}
		if(PLAN[i2]&(1<<j)){PORTB|=0x20;}else{if(PINB&0x80) e2++;}
		sleep;PORTB|=0x11;
		j++;
		if(j>7){
			j=0;
			i1++;
			i2++;
		}
		if(i1>=DEV1){
			i1=0;
			if(e1==0){PORTB&=~(0x04);ER1=0;}
			ER1++;
			e1=0;
		}
		if(i2>=(DEV1+DEV2)){
			i2=DEV1;
			if(e2==0){PORTB&=~(0x40);ER2=0;}
			ER2++;
			e2=0;
		}
	sleep;PORTB|=0x44;
	}
}

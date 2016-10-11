#include <avr/io.h>
#include <avr/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
//#include <stdio.h>

#define F_CPU 8000000L
#define MAX 20

unsigned char mess[20];
unsigned char cmess;
unsigned char number;
volatile unsigned char status;
volatile unsigned char BUF[MAX],COUNT;
volatile unsigned short COUNT_COMAND;
ISR(TIMER1_COMPA_vect)
{
	PORTD&=~(0x40);
	TCCR1B=0x00;
}
ISR(USART_RX_vect)
{
	if(status&0x80) return;
	BUF[COUNT]=UDR;
	if(UCSRA&0x1C){
		status|=0x10;
	}		
	if(status&0x20){
		status|=0x10;
	}		
	if(COUNT>=MAX)
		status|=0x10;
	if(!(status&0x10))		
		COUNT++;
	TCNT0=0x00;
	TCCR0B=0x05;
}
ISR(TIMER0_COMPA_vect)
{
	if((status&0x10)==0)
		status|=0x80;	
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

/**/void USART_Init( unsigned char i )
{
	unsigned int baudrate;
	switch (i){
		default: baudrate=103;	OCR0A=0x1C;	OCR0B=0x0C;
	}
	UBRRH = (unsigned char) (baudrate>>8);
	UBRRL = (unsigned char) baudrate;
	UCSRA = (1<<U2X);
	UCSRB = (1<<RXEN)|(1<<TXEN)|(1<<RXCIE);
	UCSRC = (0<<USBS)|(1<<UCSZ0)|(1<<UCSZ1);
} 
void USART_Transmit( unsigned char *data )
{
	while ( !(UCSRA & (1<<UDRE)) );
	UDR = *data;			        
}
void ADDCHAR(unsigned char buf){
	if(cmess>17) {return;PORTB=0xff;}
	mess[cmess]=buf;
	cmess++;
}
unsigned char CRC(unsigned char *buf,unsigned char count)
{
	unsigned short crc = 0xFFFF;
    unsigned char pos,i,h[2];
    for (pos = 0; pos < (count); pos++) {
		crc ^= (unsigned char)buf[pos];
		for (i = 8; i != 0; i--) {
			if ((crc & 0x0001) != 0) {
				crc >>= 1;
				crc ^= 0xA001;
			}
		else
			crc >>= 1;
		}
    }
	h[0]=(unsigned char)(crc>>8);
	h[1]=(unsigned char)crc;
	if((h[0]==buf[count])&&(h[1]==buf[count+1])) 
		i=1;
	else
		i=0;
	buf[count]=h[0];
	buf[count+1]=h[1];
	return i;
}
void SENDMES(void)
{
	if(BUF[0]!=number) return;
	mess[0]=number;
	CRC(&mess,cmess);
	PORTD|=0x04;
	_delay_ms(1);
	unsigned char i;
	for(i=0;i<cmess+2;i++){
		USART_Transmit(&mess[i]);
		mess[i]=0x00;
	}		
	cmess=1;	
	PORTD&=~(0x04);
}

void report_slave_id()
{
	ADDCHAR(0x11);
	ADDCHAR(12);
	ADDCHAR('2');
	ADDCHAR('3');
	ADDCHAR(0xFF);
	ADDCHAR('R');
	ADDCHAR('e');
	ADDCHAR('l');
	ADDCHAR('e');
	ADDCHAR('y');
	ADDCHAR(' ');
	ADDCHAR('1');
	ADDCHAR('.');
	ADDCHAR('0');
	SENDMES();
}
void get_com_event_counter()
{
	ADDCHAR(0x0B);
	ADDCHAR((unsigned char)(COUNT_COMAND>>8));
	ADDCHAR((unsigned char)COUNT_COMAND);
	SENDMES();
}


//unsigned short read_registr_param(unsigned char address);
//void read_registr_data(void);
//void read_bit_output(void);
//void bits_output(void);
//void write_registr_param(unsigned char address, unsigned char data);
//void write_registrs_param(void);
//void spi1(void);
//void spi2(void);

unsigned char PLAN[32];
unsigned char BLOCK[32];
unsigned char DEV1,DEV2,ER1,ER2;
int main(void)
{
/*	
	if(eeprom_read_byte(0)==0xff){
		eeprom_write_byte(0,0x00);
		eeprom_write_byte(2,0x00);
		eeprom_write_byte(3,0x00);
		eeprom_write_byte(4,0x00);
		for(i=8;i<80;i++)
			eeprom_write_byte(i,0x00);
	}
*/	
	USART_Init(eeprom_read_byte(0));
	number=eeprom_read_byte(1);
	DEV1=eeprom_read_byte(2);
	DEV2=eeprom_read_byte(3);
	ER1=0;
	ER2=0;
	DDRB= 0x77;
	DDRD= 0x74;
	PORTD=eeprom_read_byte(4)&0x30;
	for(COUNT=0;COUNT<32;COUNT++){
		BLOCK[COUNT]=eeprom_read_byte(44+COUNT);
		PLAN[COUNT]=eeprom_read_byte(8+COUNT)&BLOCK[COUNT];
	}
	COUNT=0;
	cmess=1;
	status=0;
	COUNT_COMAND=0;
	TCCR1A=0x00;
	OCR1AH=0x08;
	OCR1AL=0x00;
	TCCR0A=0x02;
	TCCR0B=0x00;
	TIMSK=(1<<OCIE0A)|(1<<OCIE0B)|(1<<OCIE1A);
	DDRB=0x77;
	sei();
	for(;;)
	{
	if(status&0x80)
		{
//			if((number==BUF[0]|BUF[0]==0x00)&&(check_CRC())){
			if((number==BUF[0]|BUF[0]==0x00)&&(CRC(&BUF,COUNT-2))){
				PORTD|=0x40;
				TCCR1B=0x0D;
				COUNT_COMAND++;
			switch(BUF[1]){
//				case 0x01:read_bit_output();break;
//				case 0x02:read_bit_input();break;
//				case 0x03:read_registr_data();break;
//				case 0x04:read_registr_data();break;
//				case 0x05:write_bit_output();break;
//				case 0x06:write_registr_param(BUF[3],BUF[5]);break;
				case 0x0B:get_com_event_counter();break;
//				case 0x0F:write_bits_output();break;
//				case 0x10:write_registrs_param();break;
				case 0x11:report_slave_id();break;
				default:
					BUF[1]|=0x80;
					BUF[2]=0x01;
					COUNT=5;
					COUNT_COMAND--;
//					send_message();					
			}			
			}
		status=0;
		COUNT=0;
		}
	_delay_ms(100);
/*	if((ER1&0xC0)==0) 
		spi1();
	if((ER2&0xC0)==0) 
		spi2();
*/	}
}

/*void read_registr_data()
{
	unsigned char i,j,begin,count;
	unsigned short RS;
	begin=BUF[3];
	count=BUF[5];
	if(count>(MAX-5)) count=(MAX-5);
	j=3;
	for(i=begin;i<(begin+count);i++)
	{
		RS=read_registr_param(i);
		BUF[j++]=(unsigned char)(RS>>8);
		BUF[j++]=(unsigned char)RS;
	}
	BUF[2]=j-3;
	COUNT=j+2;
	send_message();
}
*/
/*unsigned short read_registr_param(unsigned char address)
{
	if(address<36)
		return (unsigned char)PLAN[address];
	if(address<72)
		return (unsigned char)BLOCK[address-36];
	if(address<190)
		return (unsigned char)eeprom_read_byte(address-72);
	if(address==190)
		return (unsigned char)((PORTD&0x20)>>1)|(unsigned char)((PORTD&0x10)>>4);
	if(address==191)
//		return (ER1<<8)|(DEV1);
		return (unsigned char)DEV1;
	if(address==192)
//		return (ER2<<8)|(DEV2);
		return (unsigned char)DEV2;
}
*/
/*void write_registr_param(unsigned char address, unsigned char data)
{
	if(address<36){
		PLAN[address]=data&BLOCK[address];
		address=254;
	}		
	if(address<72){
		BLOCK[address-36]=data;
		PLAN[address-36]&=data;
		address=254;
	}
	if(address<190){
		eeprom_write_byte(address-72,data);
		address=254;
	}		
	if(address==190){
		if(data&0x01) PORTD|=0x10; 
		if(data&0x02) PORTD&=~(0x10);
		if(data&0x04) PORTD|=0x20;
		if(data&0x08) PORTD&=~(0x20);
		address=254;
	}
	if(address==191){
		DEV1=data;
		ER1=0;
		address=254;
	}		
	if(address==192){
		DEV2=data;
		ER2=0;
		address=254;
	}		
	if(address==193){
		USART_Init(data);
		address=254;
	}		
	if(address==194){
		eeprom_write_byte(1,data);
		number=data;
		address=254;
	}		
	if(address==254){
		COUNT=8;
		send_message();
	}		
}
*/
/*void write_registrs_param(void)
{
/*	unsigned char j,i,k;
	if(BUF[6]!=(COUNT-9)){
		BUF[1]|=0x80;
		BUF[2]=0x01;
		COUNT=5;
		send_message();
		return;
	}
	j=8;BUF[2]=0;
	for(i=BUF[3];i<(BUF[3]+BUF[5]);i++){
		BUF[2]+=write_registr_param(i,BUF[j]);
		j+=2;
	}		
	COUNT=5;send_message();

}
*/
/*
void read_bit_output(void)
{
	unsigned short i,j,begin,end;
	unsigned char RS,k;
	begin=(BUF[2]<<8)|BUF[3];
	end=begin+(BUF[4]<<8)|BUF[5];
	for(i=2;i<MAX;i++) BUF[i]=0;
	j=3;k=0;
	for(i=begin;i<end;i++)
	{
		RS=read_registr_param(i>>3)&(1<<(i&0x07));
		BUF[j]|=RS<<(1<<k);
		k++;
		if(k>7){k=0;j++;}
		if(j>(MAX-5)) {j--;break;}
	}
	BUF[2]=j-3;
	COUNT=j+2;
	send_message();

}
*/
/*
void spi1(void)
{
	/*
	unsigned char i,j,e;
	e=0;
	for(i=0;i<(DEV1&0x3F);i++)
		for(j=0;j<8;j++){
		PORTB&=~(0x01);
		_delay_us(10);
		if(PLAN[i]&(1<<j))
			PORTB|=0x02;
		else
			PORTB&=~(0x02);
		_delay_us(10);
		PORTB|=0x01;
		if((PORTB&(0x08)>>2)!=(PORTB&(0x02)))
			e++;
		_delay_us(10);
	}
	if(e>0){
		PORTD|=0x04;
		ER1++;
	}		
	else{
 		PORTB&=~(0x04);
		 ER1=0;
		_delay_us(10);
		PORTB|=0x04;
	}		

	}
void spi2(void)
{
/*	unsigned char i,j,e;
	e=0;
	for(i=(DEV1&0x3F);i<(DEV2&0x3F);i++)
		for(j=0;j<8;j++){
		PORTB&=~(0x10);
		_delay_us(10);
		if(PLAN[i]&(1<<j))
			PORTB|=0x20;
		else
			PORTB&=~(0x20);
		_delay_us(10);
		PORTB|=0x10;
		if((PORTB&(0x80)>>2)!=(PORTB&(0x20)))
			e++;
		_delay_us(10);
	}
	if(e>0){
		PORTD|=0x04;
		ER2++;
	}		
	else{
 		PORTB&=~(0x40);
		 ER1=0;
		_delay_us(10);
		PORTB|=0x40;
	}		
}
*/

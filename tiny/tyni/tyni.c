#include <avr/io.h>
#include <avr/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
//#include <stdio.h>

#define F_CPU 8000000L
#define MAX 32
void USART_Init(unsigned char i );
void USART_Transmit(unsigned char data );
unsigned char check_CRC(void);
unsigned short ModRTU_CRC(void);
void send_message(void);

void report_slave_id(void);
void get_com_event_counter(void);
unsigned short read_registr_param(unsigned char address);
void read_registr_data(void);
void read_bit_output(void);
void bits_output(void);
unsigned char write_registr_param(unsigned char address, unsigned char data);
void write_registrs_param(void);
void spi1(void);
void spi2(void);

unsigned char BUF[MAX];
unsigned char FACT[24];
unsigned char PLAN[24];
unsigned char BLOCK[24];
unsigned char status,COUNT,number,DEV1,DEV2;
unsigned short COUNT_COMAND;
int main(void)
{
	unsigned char i;
	if(eeprom_read_byte(0)==0xff){
		eeprom_write_byte(0,0x00);
		eeprom_write_byte(2,0x00);
		eeprom_write_byte(3,0x00);
		eeprom_write_byte(4,0x00);
		for(i=8;i<56;i++)
			eeprom_write_byte(i,0x00);
	}
	USART_Init(eeprom_read_byte(0));
	number=eeprom_read_byte(1);
	DEV1=eeprom_read_byte(2);
	DEV2=eeprom_read_byte(3);
	DDRB= 0x77;
	DDRD= 0x74;
	PORTD=eeprom_read_byte(4)&0x30;
	for(i=0;i<24;i++){
		FACT[i]=0;
		BLOCK[i]=eeprom_read_byte(32+i);
		PLAN[i]=eeprom_read_byte(8+i)&BLOCK[i];
	}
	COUNT=0;
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
			if((number==BUF[0]|BUF[0]==0x00)&&(check_CRC())){
				PORTD|=0x40;
				TCCR1B=0x0D;
				COUNT_COMAND++;
			switch(BUF[1]){
//				case 0x01:read_bit_output();break;
//				case 0x02:read_bit_input();break;
				case 0x03:read_registr_data();break;
				case 0x04:read_registr_data();break;
//				case 0x05:write_bit_output();break;
				case 0x06:write_registr_param(BUF[3],BUF[5]);COUNT=8;send_message();break;
				case 0x0B:get_com_event_counter();break;
//				case 0x0F:write_bits_output();break;
				case 0x10:write_registrs_param();break;
				case 0x11:report_slave_id();break;
				default:
					BUF[1]|=0x80;
					BUF[2]=0x01;
					COUNT=5;
					COUNT_COMAND--;
					send_message();					
			}			
			}
		status=0;
		COUNT=0;
		}
	_delay_ms(100);
//	if(DEV1&0x80) 
		spi1();
//	if(DEV2&0x80) 
		spi2();
	}
}
void report_slave_id()
{
	BUF[2]=12;
	BUF[3]='2';
	BUF[4]='3';
	BUF[5]=0xFF;
	BUF[6]='R';
	BUF[7]='e';
	BUF[8]='l';
	BUF[9]='e';
	BUF[10]='y';
	BUF[11]=' ';
	BUF[12]='1';
	BUF[13]='.';
	BUF[14]='0';
	COUNT=16;
	send_message();
}
void get_com_event_counter()
{
	BUF[2]=(unsigned char)(COUNT_COMAND>>8);
	BUF[3]=(unsigned char)COUNT_COMAND;
	COUNT=6;
	send_message();
}
void read_registr_data()
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
unsigned short read_registr_param(unsigned char address)
{
	if(address<16)
		return FACT[address];
	if(address<32)
		return PLAN[address-16];
	if(address<48)
		return BLOCK[address-32];
	if(address<176)
		return eeprom_read_byte(address-48);
	if(address==176)
		return ((PORTD&0x20)>>1)|((PORTD&0x10)>>4);
}
unsigned char write_registr_param(unsigned char address, unsigned char data)
{
	if((address>=16)&&(address<32))
		PLAN[address-16]=data&BLOCK[address-16];
	if((address>=32)&&(address<48)){
		BLOCK[address-32]=data;
		PLAN[address-32]&=data;
	}
	if((address>=48)&&(address<176))
		eeprom_write_byte(address-48,data);
	if(address==176){
		if(data&0x01) PORTD|=0x10; 
		if(data&0x02) PORTD&=~(0x10);
		if(data&0x04) PORTD|=0x20;
		if(data&0x08) PORTD&=~(0x20);
		}
	if(address==177)
		USART_Init(data);
	if(address==178){
		eeprom_write_byte(1,data);
		number=data;
		}		
	if(address>178)
		return 0;
	else
		return 1;
}
void write_registrs_param(void)
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
*/
}
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
void send_message(void)
{
	if(BUF[0]!=number) return;
	
	unsigned short CRC;
	unsigned char i;
	CRC=ModRTU_CRC();
	BUF[COUNT-2]=(unsigned char)(CRC>>8);
	BUF[COUNT-1]=(unsigned char)CRC;
	PORTD|=0x04;
	_delay_ms(1);
	USART_Transmit(number);
	for(i=1;i<COUNT;i++)
		USART_Transmit(BUF[i]);
	PORTD&=~(0x04);
}
void USART_Init( unsigned char i )
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
void USART_Transmit( unsigned char data )
{
	while ( !(UCSRA & (1<<UDRE)) );
	UDR = data;			        
}
ISR(TIMER1_COMPA_vect)
{
	PORTD&=~(0x40);
	TCCR1B=0x00;
}
ISR(USART_RX_vect)
{
	if(status&0x80) return;
	BUF[COUNT]=UDR;
	if(UCSRA&0x1C|status&0x20)
		status|=0x10;
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
		status=0x00;
	TCCR0B=0x00;
}	
ISR(TIMER0_COMPB_vect)
{
	status|=0x20;
}
unsigned short ModRTU_CRC(void)
{
	unsigned short crc = 0xFFFF;
    unsigned char pos,i;
    for (pos = 0; pos < (COUNT-2); pos++) {
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
	return crc;
}
unsigned char check_CRC()
{
	unsigned short CRC;
	CRC=ModRTU_CRC();
	return (BUF[COUNT-2]==(unsigned char)(CRC>>8))&(BUF[COUNT-1]==(unsigned char)CRC);
}
void spi1(void)
{
	unsigned char i,j;
	for(i=0;i<(DEV1&0x7F);i++)
		for(j=0;j<8;j++){
		PORTB&=~(0x01);
		_delay_us(10);
		if(PLAN[i]&(1<<j))
			PORTB|=0x02;
		else
			PORTB&=~(0x02);
		_delay_us(10);
		PORTB|=0x01;
		_delay_us(10);
	}
	PORTB&=~(0x04);
	_delay_us(10);
	PORTB|=0x04;
}
void spi2(void)
{
	
}


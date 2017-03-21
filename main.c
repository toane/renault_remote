#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#define nop() __asm__ __volatile__ ("nop \n\t")

uint8_t  read_btn(uint8_t);
uint8_t debounce(uint8_t  *button_history,uint8_t);
uint8_t getEncoderDirection(uint8_t val);

uint8_t mute_history=0;
uint8_t volp_history=0;
uint8_t volm_history=0;

volatile uint8_t emit=0;

void SetupPCM();
void Pulse (int carrier, int gap);
void SendSony (unsigned long code);
void Transmit (int address, int command) ;


//0x01: mute,0x02:volp, 0x04:volm

// Remote control
const int Address = 0x1E3A;
const int ShutterCode = 0x2D;
const int TwoSecsCode = 0x37;
const int VideoCode = 0x48;

const int top = 24;    // 1000000/25 = 40kHz
const int match = 18;  // pulses with approx 25% mark/space ratio

void setup() {
	//LEDS
	DDRB |= _BV(PB1);//led IR
	//PORTB|= _BV(PB1);//led IR allumee

	DDRB |= _BV(PB3);//led visible
	PORTB|= _BV(PB3);//led IR allumee

	sei();
	//set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	// Disable ADC to save power
	ADCSRA &= ~(1<<ADEN);
	SetupPCM();
	//SetupPinChange();
	//watchdog configuration
	WDTCR |= (1<<WDIE);//generate interrupt after each time out
}

/**
 * sets up ports to read a specific button
 * returns 1 if the selected button is pressed
 */
uint8_t read_btn(uint8_t  curbtn){
	uint8_t ret=0x00;
	if (curbtn==0x01){
		//poll Mute
		DDRB &=~_BV(PB2);//PB2 en entree
		PORTB |=_BV(PB2);//pull-up actif
		nop();nop();nop();nop();
		ret= ( (PINB & _BV(PB2)) == 0 );
	} else if (curbtn==0x02){
		//poll Volume +
		DDRB &=~_BV(PB0);//PB0 en entree
		PORTB |=_BV(PB0);//pull up sur PB0
		DDRB |=_BV(PB2);//PB2 en sortie
		PORTB &=~_BV(PB2);//PB2 a 0
		nop();nop();nop();nop();
		ret= ( (PINB & _BV(PB0)) == 0 );//lecture de PB0
	} else if (curbtn==0x04){
		//poll Volume -
		DDRB &=~_BV(PB4);
		PORTB |=_BV(PB4);
		DDRB |=_BV(PB0);
		PORTB &=~_BV(PB0);
		nop();nop();nop();nop();
		ret= ((PINB & (1<<PB4)) == 0);//lecture de PB4
	}
	return ret;
}

/***
 * returns a direction code for the last movement from the encoder (1:dir A, 2: dir B)
 * val: code for the current position(between j:0x00, b:0x01, v:0x02)
 */
uint8_t getEncoderDirection(uint8_t val){
	static const uint8_t direction[6]={0,1,2,0,1,2};
	static uint8_t hist=0xff;//rotation history

	if ((hist & 0x0F) != val){//remplace 4 premiers bits par 0, compare avec val
		hist = hist <<4;
		hist |=val;
	}

	return 0;
}

/***
 * curbtn: one of 0x01,0x02,0x04, matches mute, vol+,vol-
 * returns 1 if the button is considered pressed
 */
uint8_t debounce(uint8_t  *button_history,uint8_t  curbtn){
	uint8_t pressed = 0;
	*button_history = *button_history << 1;
	*button_history |= read_btn(curbtn);
	if ((*button_history & 0b11000111) == 0b00000111) {
		pressed=1;
		*button_history = 0b11111111;
	}
	return pressed;
}

void SetupPCM () {
	TCCR0A = 3<<COM0B0 | 3<<WGM00; // Inverted output on OC0B and Fast PWM
	TCCR0B = 1<<WGM02 | 1<<CS00;   // Fast PWM and divide by 1
	OCR0A = top;                   // 40kHz
	OCR0B = top;                   // Keep output low
}

void Pulse (int carrier, int gap) {
	int count = carrier;
	OCR0B = match;  // Generate pulses
	for (char i=0; i<2; i++) {
		for (int c=0; c<count; c++) {
			do ; while ((TIFR & 1<<TOV0) == 0);
			TIFR = 1<<TOV0;
		}
		count = gap;
		OCR0B = top;
	}
}

void SendSony (unsigned long code) {
	TCNT0 = 0;             // Start counting from 0
	// Send Start pulse
	Pulse(96, 24);
	// Send 20 bits
	for (int Bit=0; Bit<20; Bit++) {
		if (code & ((unsigned long) 1<<Bit)) Pulse(48, 24); else Pulse(24, 24);
	}
}

void Transmit (int address, int command) {
	unsigned long code = (unsigned long) address<<7 | command;
	//PORTB|= _BV(PB3);
	SendSony(code);
	_delay_ms(11);
	SendSony(code);
	//PORTB&=~ _BV(PB3);
}

int main() {
	//CLKPR = 0x80;
	//CLKPR = 0 ;  // presc 1
	setup();
	while(1){
		// Go to sleep
		if (emit==0x01){
			emit=0x00;
			Transmit(Address, ShutterCode);
		}
		//sleep_enable();
		//sleep_cpu();
	}
}

ISR (WDT_vect){

	if (debounce(&volp_history,0x02)==1){
		PORTB ^= _BV(PB3);//flip led 2

	}

	if (debounce(&mute_history,0x01)==1){
		//PORTB &= ~_BV(PB1);//turn off
		//PORTB &= ~_BV(PB3);//turn off
		emit=0x01;
	}

	if (debounce(&volm_history,0x04)==1){
		PORTB ^= _BV(PB1);//flip led 2
	}

}

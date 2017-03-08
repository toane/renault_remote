#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

/*
uint8_t read_button_mute();
uint8_t read_button_volp();
uint8_t  read_button_volm();
*/
uint8_t  read_btn();
uint8_t test_for_press_only(void);
uint8_t test_for_press_two(void);
#define nop() __asm__ __volatile__ ("nop \n\t")



//01: mute, 02:volp, 03:volm
volatile uint8_t curbtn=0x00;

void setup() {
	//LEDS
	DDRB |= _BV(PB1);//led IR
	PORTB|= _BV(PB1);//led IR allumee
	DDRB |= _BV(PB3);//led visible
	PORTB |= _BV(PB3);//led visible allumee

	sei();
	//set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	// Disable ADC to save power
	ADCSRA &= ~(1<<ADEN);
	//SetupPCM();
	//SetupPinChange();
	//watchdog configuration
	WDTCR |= (1<<WDTIE);//generate interrupt after each time out
}

uint8_t read_btn(){
	uint8_t ret=0x00;

	if (curbtn==0x00){//mute
		DDRB &=~_BV(PB2);//PB2 en entree
		PORTB |=_BV(PB2);//pull-up actif
		ret=( (PINB & _BV(PB2)) == 0 );
	}else if (curbtn==0x02){//volp
		DDRB |=_BV(PB2);//PB2 en sortie
		PORTB &=~_BV(PB2);//PB2 a 0
		DDRB &=~_BV(PB0);//PB0 en entree
		PORTB |=_BV(PB0);//pull up sur PB0
		ret= ( (PINB & _BV(PB0)) == 0 );
	}else if (curbtn==0x03){//volm
		DDRB |=_BV(PB0);//PB0 en sortie
		PORTB &=~_BV(PB0);//PB0 a 0
		DDRB &=~_BV(PB4);//PB4 en entree
		PORTB |=_BV(PB4);//pull up sur PB4
		ret= ((PINB & (1<<PB4)) == 0);//lecture de PB0
	}
	return ret;
}


/*
uint8_t read_button_mute(){
	DDRB &=~_BV(PB2);//PB2 en entree
	PORTB |=_BV(PB2);//pull-up actif
	return ( (PINB & _BV(PB2)) == 0 );
}

uint8_t  read_button_volp(){
	DDRB |=_BV(PB2);//PB2 en sortie
	PORTB &=~_BV(PB2);//PB2 a 0
	DDRB &=~_BV(PB0);//PB0 en entree
	PORTB |=_BV(PB0);//pull up sur PB0
	return ( (PINB & _BV(PB0)) == 0 );
}

uint8_t  read_button_volm(){
	//VOLM
	DDRB |=_BV(PB0);//PB0 en sortie
	PORTB &=~_BV(PB0);//PB0 a 0
	DDRB &=~_BV(PB4);//PB4 en entree
	PORTB |=_BV(PB4);//pull up sur PB4
	return ((PINB & (1<<PB4)) == 0);//lecture de PB0
}
*/

uint8_t test_for_press_only(void){
	static uint8_t button_history = 0;
	uint8_t pressed = 0;
	button_history = button_history << 1;
	button_history |= read_button_btn();
	if ((button_history & 0b11000111) == 0b00000111) {
		pressed = 1;
		button_history = 0b11111111;
	}
	return pressed;
}

int main() {
	setup();
	while(1){
		// Go to sleep
		//Transmit(Address, ShutterCode);
		//sleep_enable();
		//sleep_cpu();
	}
}

ISR (WDT_vect){
	if (test_for_press_only()==1){
		PORTB ^= _BV(PB3);//flip led 1
	}
}

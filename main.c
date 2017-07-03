#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#define nop() __asm__ __volatile__ ("nop \n\t")
//IR code from http://www.technoblogy.com/show?VFT

//Wired remote settings
#define REMOTEPORT PORTB
#define REMOTEDDR DDRB
#define REMOTEPIN PINB
#define REMOTE_RED PB0
#define REMOTE_BLUE PB2
#define REMOTE_BROWN PB3
#define REMOTE_YELLOW PB4
#define DAMPSIZE 16

//time settings
#define ONE_ACTIVE 660
#define ONE_PAUSE 1470
#define  ZERO_ACTIVE 640
#define  ZERO_PAUSE 400

void pulse_zero();
void pulse_one();
void sendCode(uint8_t code);
void preamble();
void transmit(uint8_t address,uint8_t code);
uint8_t checkEqualValues(unsigned char *arr,int size);
void updateWheel(uint8_t val);
uint8_t  read_btn(uint8_t);
uint8_t debounce(uint8_t  *button_history,uint8_t);

uint8_t mute_history=0;
uint8_t volp_history=0;
uint8_t volm_history=0;
uint8_t src1_history=0;

//rotary switch values
const uint8_t RIGHT=1;
const uint8_t LEFT=2;
const uint8_t PRSJ=0x00;
const uint8_t PRSB=0x01;
const uint8_t PRSV=0x02;
uint8_t  turnDirection=0;//is given a control code when detecting a wheel movement in either direction

//0x01: mute,0x02:volp, 0x04:volm

volatile uint8_t emit=0;
// Remote control
//const int JVC_ADDRESS = 0xF1;
const int JVC_ADDRESS = 0x0F;
const int JVC_VOLP = 0x21;
const int JVC_VOLM = 0xA1;
const int JVC_MUTE = 0x71;
const int JVC_SRC1 = 0xFF;//0x11;//TODO
const int JVC_F = 0x49;
const int JVC_R = 0xC9;

void setup() {
	//LEDS
	DDRB |= _BV(PB1);//led IR
	PORTB &= ~_BV(PB1);//led IR eteinte

	DDRB &=~ _BV(REMOTE_BROWN);
	REMOTEPORT|= _BV(REMOTE_BROWN);//pull up sur PB3 (marron, contact commun molette)

	sei();
	//set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	ADCSRA &= ~(1<<ADEN);
	//watchdog configuration
	WDTCR |= (1<<WDIE);//generate interrupt after each time out
}

void pulse_zero () {
	PORTB |=_BV(PB1);
	_delay_us(ZERO_ACTIVE);
	PORTB &=~_BV(PB1);
	_delay_us(ZERO_PAUSE);
}

void pulse_one () {
	PORTB |=_BV(PB1);
	_delay_us(ONE_ACTIVE);
	PORTB &=~_BV(PB1);
	_delay_us(ONE_PAUSE);
}

void preamble(){
	PORTB |=_BV(PB1);
	_delay_us(8440);
	PORTB &=~_BV(PB1);
	_delay_us(4220);
}

void sendCode (uint8_t code) {
	for (int Bit=7; Bit>-1; Bit--) {
		if (code & ((unsigned long) 1<<Bit)) pulse_one(); else pulse_zero();
	}
}

void transmit(uint8_t address,uint8_t code){
	preamble();
	sendCode(address);
	//sendCode(code);
}

/**
 * checks if arr contains identical values
 * */
uint8_t checkEqualValues(unsigned char *arr,int n) {
	uint8_t ret=1;
	for(int i = 0; i < n - 1; i++)
	{
		if(arr[i] != arr[i + 1])
			ret = 0;
	}
	return ret;
}

/***
 * sets a global variable keeping the last movement from the encoder (1:dir A, 2: dir B)
 * val: code for the current position(between j:0x00, b:0x01, v:0x02)
 * transitions: Sens A( J->B, B->V, V-> J), Sens B (V->B, B->J, J->V)
 */
void updateWheel(uint8_t poscode){
	static uint8_t hist=0xff;//rotation history
	static unsigned char damp[DAMPSIZE];
	static unsigned char didx=0;
	static unsigned char add=0x0f;
	damp[didx]=poscode;
	if (checkEqualValues(damp,DAMPSIZE)){
		add=damp[DAMPSIZE-1];
	}
	if (didx>=DAMPSIZE-1){
		didx=0;
	}
	else {
		didx++;
	}

	if ((hist & 0x0F) != add){//compare les 4 derniers bits de hist avec val
		hist = hist <<4;
		hist |=add;
		switch(hist){
		case 2:turnDirection=JVC_F;break; // J -> V
		case 16:turnDirection=JVC_F;break;// B -> J
		case 33:turnDirection=JVC_F;break;// V -> B

		case 1:turnDirection=JVC_R;break;// J -> B
		case 18:turnDirection=JVC_R;break;//B -> V
		case 32:turnDirection=JVC_R;break;//V -> J

		default:turnDirection=0;//in doubt, do nothing
		}
		//emit=turnDirection;;
	}
}

/**
 * sets up ports to read a specific button
 * returns 1 if the selected button is pressed
 */
uint8_t read_btn(uint8_t  curbtn){
	uint8_t ret=0x00;
	uint8_t rotPosPRSV=PRSV;//toggled to 0 if wheel switch in PRSJ or PRSB position
	if (curbtn==0x01){
		//poll Mute
		REMOTEDDR &=~_BV(REMOTE_BLUE);//PB2 en entree
		REMOTEPORT |=_BV(REMOTE_BLUE);//pull-up actif
		nop();nop();nop();nop();
		ret= ( (REMOTEPIN & (1<<REMOTE_BLUE)) == 0 );
	} else if (curbtn==0x02){
		//poll Volume +
		REMOTEDDR &=~_BV(REMOTE_RED);//PB0 en entree
		REMOTEPORT |=_BV(REMOTE_RED);//pull up sur PB0
		REMOTEDDR |=_BV(REMOTE_BLUE);//PB2 en sortie
		REMOTEPORT &=~_BV(REMOTE_BLUE);//PB2 a 0
		nop();nop();nop();nop();
		ret= ( (REMOTEPIN & (1<<REMOTE_RED)) == 0 );//lecture de PB0
	} else if (curbtn==0x04){
		//poll Volume -
		REMOTEDDR &=~_BV(REMOTE_YELLOW);
		REMOTEPORT |=_BV(REMOTE_YELLOW);
		REMOTEDDR |=_BV(REMOTE_RED);
		REMOTEPORT &=~_BV(REMOTE_RED);
		nop();nop();nop();nop();
		ret= ((REMOTEPIN & (1<<REMOTE_YELLOW)) == 0);//lecture de PB4 WHAT IT THIS TODO
		//ret= ((REMOTEPIN & _BV(REMOTE_RED)) == 0);//lecture de PB4 WHAT IT THIS TODO
	} else if (curbtn==0x03){
		//poll Source 1
		REMOTEDDR |=_BV(REMOTE_RED);
		REMOTEPORT |=_BV(REMOTE_RED);//rouge a 1
		REMOTEDDR &=~_BV(REMOTE_YELLOW);
		REMOTEPORT |=~_BV(REMOTE_YELLOW);//inchangé
		nop();nop();nop();nop();
		ret= ((REMOTEPIN & (1<<REMOTE_YELLOW)) == 0);//si rouge a 1 et yellow a 0 -> yellow a la masse-> source 1 presse
	}

	REMOTEDDR |=_BV(REMOTE_YELLOW);//PB4 en sortie
	REMOTEDDR |=_BV(REMOTE_BLUE);//PB2 en sortie

	REMOTEPORT &=~_BV(REMOTE_YELLOW);//PB4 a 0
	REMOTEPORT |=_BV(REMOTE_BLUE);//PB2 a 1
	nop();nop();

	if ((REMOTEPIN & (1<<REMOTE_BROWN)) == 0 ){
		updateWheel(PRSJ);
		rotPosPRSV=0;
	}

	REMOTEPORT &=~_BV(REMOTE_BLUE);//PB2 a 0
	REMOTEPORT |=_BV(REMOTE_YELLOW);//PB4 a 1
	nop();nop();
	if ((REMOTEPIN & (1<<REMOTE_BROWN)) == 0 ){
		updateWheel(PRSB);
		rotPosPRSV=0;
	}

	//if wheel not in PRSB or PRSJ position we assume it's on the third position
	if (rotPosPRSV==PRSV){
		updateWheel(PRSV);
	}
	return ret;
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


int main() {
	//CLKPR = 0x80;
	//CLKPR = 0 ;  // presc 1
	setup();
	uint8_t dir =0;
	while(1){
		if (emit>0){
			dir=emit;
			emit=0;
			transmit(JVC_ADDRESS,dir);
		}

		if (turnDirection>0){
			dir=turnDirection;
			turnDirection=0;
			transmit(JVC_ADDRESS,dir);
		}

	}
	/*
		if (emit>0) {
			dir=emit;
			emit=0;
			for (	uint8_t i=0; i<dir; i++) {
				PORTB&=~_BV(PB1);//eteindre led
				_delay_ms(200);
				PORTB|=_BV(PB1);//allumer led
				_delay_ms(150);
			}
		}
	}
	 */
}

ISR (WDT_vect){
	if (debounce(&volp_history,0x02)==1){
		emit=JVC_VOLP;
	}

	if (debounce(&mute_history,0x01)==1){
		emit=JVC_VOLM;
	}

	if (debounce(&volm_history,0x04)==1){
		emit=JVC_MUTE;
	}

	if (debounce(&src1_history,0x05)==1){
			emit=JVC_SRC1;
		}

}



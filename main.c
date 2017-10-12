#define F_CPU 1000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#define nop() __asm__ __volatile__ ("nop \n\t")

//PWM code from http://www.technoblogy.com/show?VFT
//Wired remote settings
#define REMOTEPORT PORTB
#define REMOTEDDR DDRB
#define REMOTEPIN PINB
#define REMOTE_RED PB0
#define REMOTE_BLUE PB2
#define REMOTE_BROWN PB3
#define REMOTE_YELLOW PB4
#define DAMPSIZE 16

void sendCode(uint16_t code);
void pulse(int carrier, int gap);
void preamble();
void transmit(uint8_t address,uint8_t code);
uint8_t checkEqualValues(unsigned char *arr,int size);
void updateWheel(uint8_t val);
uint8_t  read_btn(uint8_t);
uint8_t debounce(uint8_t  *button_history,uint8_t);
void setupPWM () ;
void stopPWM () ;
void check_inputs_fn(void);

uint8_t mute_history=0;
uint8_t volp_history=0;
uint8_t volm_history=0;
uint8_t src1_history=0;

//wheel switch
const uint8_t PRSJ=0x00;
const uint8_t PRSB=0x01;
const uint8_t PRSV=0x02;
volatile uint8_t  turnDirection=0;//is given a control code when detecting a wheel movement in either direction (JVC_R or JVC_F )
volatile uint8_t emit=0;
volatile uint8_t check_inputs = 0;

//Command constants

const uint8_t JVC_ADDRESS = 0xF1;
const uint8_t JVC_VOLP = 0x21;
const uint8_t JVC_VOLM = 0xA1;
const uint8_t JVC_MUTE = 0x71;
const uint8_t JVC_SRC1 = 0x11;
const uint8_t JVC_F = 0xC9;
const uint8_t JVC_R = 0x49;


void setup() {
	DDRB |= _BV(PB1);//PB1 en sortie,

	DDRB &=~ _BV(REMOTE_BROWN);
	REMOTEPORT|= _BV(REMOTE_BROWN);//pull up sur PB3 (marron, contact commun molette)
	sei();
	//AVR Manual: WDT Timeout is 4 or 64 ms
	//watchdog configuration
	WDTCR |= (1<<WDIE);//generate interrupt after each time out
	//TCNT0=0;
}

void setupPWM () {
	//fast pwm, non inverting, prescaler 8
	TCCR0A = (1<<WGM01)| (1<<WGM00)|(1<<COM0B1);
	TCCR0B = (1<<WGM02)|( 1<<CS01);
}

void stopPWM () {
	TCCR0A =0;
}

void preamble(){
	PORTB |=_BV(PB1);
	_delay_us(8600);
	PORTB &=~_BV(PB1);
	_delay_us(3100);
}

void sendCode (uint16_t code) {
	TCNT0=0;

	OCR0A=0;
	OCR0B=0;
	do ; while ((TIFR & 1<<OCF0B) == 0);
	TIFR = 1<<OCF0B;

	OCR0A=0;
	OCR0B=0;
	do ; while ((TIFR & 1<<OCF0B) == 0);
	TIFR = 1<<OCF0B;

	//for (int Bit=17; Bit>-1; Bit--) {//weird loop indexes
		for (uint16_t Bit=0x8000;Bit;Bit=Bit>>1){
		//if (code & (1<<Bit)) {
			if (code & Bit) {
			OCR0A=255;
			OCR0B=80;
		}
		else {
			OCR0A=130;
			OCR0B=80;
		};
		//wait for TIFR:OCF0B==1
		//The OCF0B bit is set when a Compare Match occurs between the Timer/Counter
		//and the data in OCR0B
		do ; while ((TIFR & 1<<OCF0B) == 0);
		//clear OCF0B by writing a 1
		TIFR = 1<<OCF0B;
	}
	//stop bit (short pulse)
	OCR0A=130;
	OCR0B=80;
	do ; while ((TIFR & 1<<OCF0B) == 0);
	TIFR = 1<<OCF0B;

}

void transmit(uint8_t address,uint8_t code){
	cli();
	preamble();
	setupPWM();
	sendCode((address<<8)+code);
	stopPWM();
	sei();
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
		nop();nop();nop();nop(); //nops make sure the ports have time to settle to their new state before testing
		ret= ((REMOTEPIN & (1<<REMOTE_YELLOW)) == 0);
	} else if (curbtn==0x03){
		//poll Source 1
		REMOTEDDR |=_BV(REMOTE_RED);
		REMOTEPORT |=_BV(REMOTE_RED);//rouge a 1
		REMOTEDDR &=~_BV(REMOTE_YELLOW);
		REMOTEPORT |=~_BV(REMOTE_YELLOW);//inchangé
		nop();nop();nop();nop();
		ret= ((REMOTEPIN & (1<<REMOTE_YELLOW)) == 0);//si rouge a 1 et yellow a 0 -> yellow a la masse-> source 1 presse
	}

	//test if wheel in brown-yellow position (PRSJ)
	REMOTEDDR |=_BV(REMOTE_YELLOW);//PB4 en sortie
	REMOTEDDR |=_BV(REMOTE_BLUE);//PB2 en sortie
	REMOTEPORT &=~_BV(REMOTE_YELLOW);//PB4 a 0
	REMOTEPORT |=_BV(REMOTE_BLUE);//PB2 a 1
	nop();nop();
	if ((REMOTEPIN & (1<<REMOTE_BROWN)) == 0 ){
		updateWheel(PRSJ);
		rotPosPRSV=0;
	}

	//test if wheel in brown-blue position (PRSB)
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
	//CLKPR = 1 ;  // presc 2
	setup();
	//the first command comes out mangled so we send a bogus message
	//to get it over with
	transmit(0x00,0x00);
	uint8_t dir =0;
	while(1){
		cli();
		if(check_inputs){
			sei();
			check_inputs_fn();
			if (emit>0){
				dir=emit;
				emit=0;
				transmit(JVC_ADDRESS,dir);
				transmit(JVC_ADDRESS,dir);
			}

			if (turnDirection>0){
				dir=turnDirection;
				turnDirection=0;
				transmit(JVC_ADDRESS,dir);
				transmit(JVC_ADDRESS,dir);
			}
			cli();	//Not sure if this is truly necessary.  We might be able to just
				//Treat check_inputs as a boolean (and change the ISR respectively)
			check_inputs = check_inputs == 0 ? 0 : check_inputs-1;
			sei();
		}
		else{
			sei();
		}
	}
}

void check_inputs_fn(void)
{
	if (debounce(&volp_history,0x02)==1){
		emit=JVC_VOLP;
	}

	if (debounce(&mute_history,0x01)==1){
		emit=JVC_MUTE;
	}

	if (debounce(&volm_history,0x04)==1){
		emit=JVC_VOLM;
	}
/*
	if (debounce(&src1_history,0x03)==1){
		emit=JVC_SRC1;
	}
*/
}

ISR (WDT_vect){
	check_inputs++;
}

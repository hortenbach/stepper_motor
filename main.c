/***********************************************************************************
* ARM Programmierung WiSe 19/20 
* Project Stepper Motor
* 
* Date					version			User			comment
************************************************************************************
* 10-04-20				2.0				Urs				Merge and fixed Timers
************************************************************************************
* 12-04-20 				2.1				Gina			Insert header to keep track of latest main 	
************************************************************************************
*
*****************************/
#include <stdio.h>
#include <math.h>
#include <LPC23xx.h>                    /* LPC23xx definitions                */
#include "linear_acceleration.h"

/***********************************
* USER INPUT
************************************/

/* How many steps do you want to move? */
#define DISTANCE 200


/***********************************
* PINS
************************************/

#define RESET 	BIT0	// P3.24
#define ENABLE 	BIT7	// P3.23
#define SLEEP 	BIT1	// P3.25
#define STEP 		BIT2 	// P3.26	

#define MS1 BIT2	// P4.2
#define MS2 BIT1	// P4.1
#define MS3 BIT0	// P4.0
#define DIR	BIT4    // P4.4

/***********************************
* Timer
************************************/

#define INT_TMR0	04
#define INT_TMR1	05
#define INT_TMR2	26
#define INT_TMR3	27
 
 /***********************************
* Helpers
************************************/

#define BIT0 0x01
#define BIT1 0x02
#define BIT2 0x04
#define BIT3 0x08
#define BIT4 0x10
#define BIT5 0x20
#define BIT6 0x40
#define BIT7 0x80

#define RIGHT 1
#define LEFT	0
/***********************************
* GLOBAL VARIABLES
************************************/

/* Global Variables for linear acceleration */
	static double cycles = 0.0;
	
	static int stepcnt = 0;
	static int acc = 1;
	static double peakCycles, breakCycles, total;
	int breakSteps;
	long totalaccCycles = 2565239; 	//first delay
	int accSteps = 1;					/* how many steps we need/have for acceleration and breaking */

/***********************************
* FUNCTION DECLARATION
************************************/

void __irq T0ISR(void);
void __irq T1ISR(void);
void __irq T2ISR(void);
void __irq T3ISR(void);

static void InitTimer0(void);
static void InitTimer1(void);
static void InitTimer2(void);
static void InitTimer3(void);

void MotorControlPinConfiguration(void);


 /***********************************
* MAIN
************************************/
int main (){		

	/* set leds */
	/* function 0, clear last 16 bit*/
	PINSEL4 &= ~0x0000FFFF;
	/* Important: always use Fast IO */
	FIO2DIR  = 0x000000FF;                /* P2.0..7 defined as Outputs         */
  FIO2MASK = 0x00000000;
	
	double noChange = 0;			/* amount of steps we drivein max speed */

	peakCycles 	= 2565239;		//first delay
	
	/* calculate max speed for desired distance */
	while(accSteps < (DISTANCE/2)){	/* after half the way we need to start slowing down again */
		peakCycles = cntVal(peakCycles, accSteps, acc);
		if (peakCycles <= MAX_SPEED){	/* we are not half way but reached maximum speed */
			peakCycles = MAX_SPEED;															
			break;
		}
		accSteps++;
		totalaccCycles += peakCycles;
	}

	/* how long do we have constant speed? */
	if (peakCycles == MAX_SPEED){
		noChange = DISTANCE - 2 * accSteps;							/* steps we spend in constant (max) speed */
		breakCycles = (noChange + totalaccCycles);		  /* at this time we need to slow down */
	} else {
		breakCycles = totalaccCycles;
	}
	total = totalaccCycles + breakCycles;			/* total cycles from beginning to end */
	breakSteps = DISTANCE-accSteps;
	
	/* config timer */
	InitTimer0();
	InitTimer1();
	InitTimer2();
//	InitTimer3();
	
	/* config motor */
	MotorControlPinConfiguration();

	while(1){
		if(stepcnt >= accSteps){
			acc = 0;
		} else if (stepcnt >= breakSteps){
			acc = -1;
		} else {
			acc = 1;
		}
	}
		
}


 /***********************************
* FUNCTION DEFINITIONS
************************************/

/* ISR ROUTINES */

void __irq T0ISR() {
	T0TCR = 0x02; // Counter Reset
	
//	VICVectAddr4 = (unsigned long)T0ISR;
//	VICVectPriority4 = 0;
//	VICIntEnable |= 1 << INT_TMR0;
	cycles = cntVal(T0MR0, stepcnt, acc);
		
	T0MR0 = (int)cycles;
	T1MR0	= (int)cycles/2;
	
	FIO3SET3 = STEP; 			/* Step HIGH */
	FIO2SET |= BIT1;
	
	T0IR = 0x01;			/* Clear interrupt flag */
	VICVectAddr = 0;	/* Acknowledge Interrupt */
	
	T0TCR = 0x01;	// enable Timer0;
	T1TCR = 0x01; // enable Timer1;
}

void __irq T1ISR() {
	T1TCR = 0x02; // Counter Reset
	
 	if (acc ==  1 || acc == 0) { stepcnt++; }
	if (acc == -1) { stepcnt --;}

	FIO3CLR3 = STEP;			/* Step LOW */
	FIO2CLR |= BIT1;

	T1IR = 0x01;			/* Clear interrupt flag */
	VICVectAddr = 0;	/* Acknowledge Interrupt */
}

/* reached destination, drive back */
void __irq T2ISR() {
	T2TCR = 0x02; // Counter Reset

	stepcnt = 1;
	cycles = 0.0;

	T0MR0 = FIRSTDELAY;
	T1MR0 = T0MR0/2;
	T2MR0 = total;
	
	T2MCR = 0x03;
	T2IR = 0x01;
	VICVectAddr = 0;
}

void __irq T3ISR() {
	T3TCR = 0x02; // Counter Reset	

	T3IR = 0x01;
	VICVectAddr = 0;
	T3TCR = 0x01; 

}
/* TIMER INITIALISATIONS */
static void InitTimer0(void){
	T0TCR = 0x02;
	T0MR0 = FIRSTDELAY;
	
	T0MCR = 0x03; // interrupt on MR0; reset on MR0; 
	//T0MCR = 0x07; // interrupt on MR0; reset on MR0; stop on MR0
	
	VICVectAddr4 = (unsigned long)T0ISR;
	VICVectPriority4 = 0;
	VICIntEnable |= 1 << INT_TMR0;
 
	T0TCR = 0x01;
}

static void InitTimer1(void){
	T1TCR = 0x02;
	T1MR0 = T0MR0/2;
	
	T1MCR = 0x03; // interrupt on MR0; reset on MR0; 
	//T1MCR = 0x7; // interrupt on MR0; reset on MR0; stop on MR0
	
	VICVectAddr5 = (unsigned long)T1ISR;
	VICVectPriority4 = 1;
	VICIntEnable |= 1 << INT_TMR1;
 
	T1TCR = 0x01;
}

static void InitTimer2(void){
	PCONP |= 0x1 << 22;			/* power timer 2 */

	T2TCR = 0;
	T2MR0 = total;
	
	T2MCR = 0x03;
	
	VICVectAddr26 = (unsigned long)T2ISR;
	VICVectPriority4 = 0;
	VICIntEnable |= 1 << INT_TMR2;
 
	T2TCR = 0x01;
}


static void InitTimer3(void){
	PCONP |= 0x1 << 23;			/* power timer 3 */

	T3TCR = 0;
//	T3MR0 = breakCycles;
	
	T3MCR = 0x03;
	
	VICVectAddr27 = (unsigned long)T3ISR;
	VICVectPriority5 = 0;
	VICIntEnable |= 1 << INT_TMR3;
 
	T3TCR = 0x01;
}
/* MOTOR CONFIGURATIONS */

void MotorControlPinConfiguration(void){
	/* Pin Function */
	/* 00 = GPIO */
		PINSEL3 &= 0x00000000;		// P1.16..31	
		PINSEL6 &= 0x00005555; 		// P3.00..15
		PINSEL7 &= 0x00000000;		// P3.16..31 <--
		PINSEL8 &= 0x55555400;		// P4.00..15 <--
		PINSEL9 &= 0x50090000;		// P4.16..31
		//PINSEL10 &= 0x00000008;		// ETM Interface
		PINSEL10 &= 0x00000000;		// ETM Interface disabled
	
	/* Pull-Up/-Down Resistors */
	/* 	00 = Pull-Up, 10 = no Resistor, 11 = Pull-Down  */
		PINMODE3 = 0x00000000;		// P1.16..31
		PINMODE7 = 0x00000000; 		// P3.16..31
		PINMODE8 = 0x00000000;		// P4.00..15
	
	/* Input / Output */
		FIO4DIR0 |= DIR + MS1 + MS2 + MS3; 	// P4.00..07
		FIO3DIR2 |= ENABLE;									// P3.16..23
		FIO3DIR3 |= SLEEP + RESET + STEP;		// P3.24..31
	
	/* Pins maskieren */
		FIO3MASK2 	= 0x00;	// P3.16..23
		FIO3MASK3 	= 0x00;	// P3.24..31
		FIO4MASK0 	= 0x00;	// P4.00..07
		
		/* Pins setzen */
		FIO3CLR2 |= ENABLE;			/* Enable ist low Active */
		FIO3SET3 |= SLEEP;			/* Sleep ist low Active */
		FIO3SET3 |= RESET;			/* Reset ist low Active */
		
		/* (DEBUG) set LED */
		FIO2CLR |= BIT3;			/* Enable ist low Active */
		FIO2SET |= BIT2;			/* Sleep ist low Active */
		FIO2SET |= BIT0;			/* Reset ist low Active */
		
		
	/* Schrittweite festlegen */
		//FIO4SET0 |= MS1 + MS2 + MS3; // Sixteenth Step 
		FIO4CLR0 |= MS1 + MS2 + MS3; // Full Step
		/* (DEBUG) set LED */
		FIO2CLR |= BIT7 | BIT6 | BIT5;			/* Enable ist low Active */
		
		
	/* Richtung festlegen */
		FIO4SET0 |= DIR; // rotate right
		//FIO4CLR0 = DIR; // rotate left
		/* (DEBUG) set LED */
		FIO2SET |= BIT4;			/* Enable ist low Active */

}

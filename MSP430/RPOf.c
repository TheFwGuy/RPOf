/**
 *  @file RPOf.c 
 *  Copyright (c) 2014 - Stefano Bodini
 *
 *  This code is designed to act as On/Off pushbutton to correctly power up and power down 
 *  a Raspberry Pi
 *  The board is a LaunchPad board, using a MSP430F2012 and the code is compiled using MSPGCC 4.4.6
 *  using the -mmcu=msp430f2012  parameter
 */
#include <msp430.h>
#include <legacymsp430.h>

/*
 *  Defines for I/O
 *   P1.0  -> Power LED  (OUTPUT - red LED)
 *   P1.1  -> Shutdown signal (OUTPUT)
 *   P1.2  -> Power Relay (OUTPUT)
 *   P1.3  -> Pushbutton (INPUT)
 *   P1.4  -> Shutdown confirmation (INPUT)
 *   P1.5  -> not used - input
 *   P1.6  -> Debug LED (OUTPUT)
 *   P1.7  -> Generic Debug  (OUTPUT)
 */
#define     RPOF_PORT      P1OUT
#define     PB_MASK        0x18  /* 0001 1000 */

#define		PORTIO		0xC7		/*  I/O defines 
											 *  0 - input
											 *  1 - output
											 *  1100 0111
											 */

#define		LED			BIT0
#define		SHTDOUT		BIT1
#define		RELAY			BIT2
#define		PUSHBTN		BIT3
#define		SHTDIN		BIT4
#define		LED_DBG		BIT6
#define		GEN_DBG		BIT7


#define		IDLE					0
#define		POWERON_START		1
#define		POWERON				2
#define		POWEROFF_START		3
#define		POWEROFF				4

#define     TMRVALUE          160    /* Timer will generate a interrupt every 10ms */
#define		FLASHTIME			50000		/* Value to obtain .5 sec from the 10 ms timer */
#define		SAFETIME				50000		/* Value to obtain .5 sec from the 10 ms timer */
#define		TURNOFFTIME			100		/* 50 seconds (100) */

#define		OFF				0
#define 		ON 				1
#define		FLASH				2
/*
 *  Functions prototypes
 */
unsigned char readPushbutton();
unsigned char readShutdownFeedback();
void LEDPower();
void Relay();
 
/*
 *  Global variables
 */
unsigned char stateMachine = IDLE;
unsigned char LEDStatus    = OFF;
unsigned char RelayStatus  = OFF;
int TimeFlash              = 0;
int SafetyTimer            = 0;
int OFFTimer               = 0;

int main(void)
{
	unsigned char buttonOnOff;
	
   WDTCTL = WDTPW + WDTHOLD;	  /* Stop WDT */
   /*
    *  Halt the watchdog timer
    *  According to the datasheet the watchdog timer
    *  starts automatically after powerup. It must be
    *  configured or halted at the beginning of code
    *  execution to avoid a system reset. Furthermore,
    *  the watchdog timer register (WDTCTL) is
    *  password protected, and requires the upper byte
    *  during write operations to be 0x5A, which is the
    *  value associated with WDTPW.
    */

   /*
    *  Set DCO
    */
   DCOCTL  = CALDCO_16MHZ;
   BCSCTL1 = CALBC1_16MHZ;       /* Set up 16 Mhz using internal calibration value */

   /*
    *  Set I/O port
    *  CAUTION ! Since are used the internal pull up resistors, remember
    *  to force the corresponded PxOUT bits, otherwise the internal resistor
    *  is not connected !!
    */
   P1DIR = PORTIO;              /* Set up port 1 for input and output */
   P1OUT = 0;                   /* Force output to zero and input pins to 1 (for pull-up resistors) */
//   P1REN = PB_MASK;             /* Set up pull up resistor for buttons !!!! WRONG Investigate */
   P1SEL = 0;                   /* All I/O */
   
   /*
    *  Set Timer
    */
   TACTL = TASSEL_2 + MC_1;    /* Uses SMCLK, count in up mode */
   TACCTL0 = CCIE;             /* Use TACCR0 to generate interrupt */
   TACCR0 = TMRVALUE;          /* Approx .01 ms */

   /*  NORMAL MODE */
   TACCTL0 &= ~0x0080;         /* Disable Out0 */

   _BIS_SR(GIE);               /* Enable interrupt */
   eint();	//Enable global interrupts

   while(ON)
   {
		LEDPower();								/* Handle Power LED status */
		Relay();									/* Handle Relay */
		buttonOnOff = readPushbutton();	/* Read pushbutton */

		switch(stateMachine)
		{
			case IDLE:
				if(buttonOnOff == ON)
				{
					stateMachine = POWERON_START;
//					RPOF_PORT |= LED_DBG;	/* Turn ON debug LED */
				}
				break;
			
			case POWERON_START:
				if(buttonOnOff == OFF)	/* Wait for the release of the pushbutton */
				{
					LEDStatus    = ON;			/* Turn ON pushbutton LED */
					RelayStatus  = ON;			/* Turn ON Relay */
					stateMachine = POWERON;
//					RPOF_PORT &= ~LED_DBG;	/* Turn OFF debug LED */
				}
				break;

			case POWERON:
				if(buttonOnOff == ON)
				{
					stateMachine = POWEROFF_START;
					LEDStatus    = FLASH;
//					RPOF_PORT |= LED_DBG;	/* Turn ON debug LED */
				}
				break;

			case POWEROFF_START:
				if(buttonOnOff == OFF)	/* Wait for the release of the pushbutton */
				{
					stateMachine = POWEROFF;
					OFFTimer = TURNOFFTIME;		/* Start turning OFF timer */
//					RPOF_PORT &= ~LED_DBG;	/* Turn OFF debug LED */
				}
				break;

			case POWEROFF:
				if(readShutdownFeedback() || OFFTimer == 0)
				{
					LEDStatus    = OFF;			/* Turn OFF pushbutton LED */
					RelayStatus  = OFF;			/* Turn OFF Relay */
					OFFTimer     = 0;				/* Stop turning OFF Timer */
					stateMachine = IDLE;
				}
				break;
			
			default:
				stateMachine = IDLE;
				break;
		}
	}
}

/*
 *  Read pushbutton
 *  Return 1 if the pushbutton is pressed, 0 otherwise
 */ 
unsigned char readPushbutton()
{
	unsigned short index;
	
	if(!(P1IN & PUSHBTN))
	{
		for(index=0; index < 2000; index++);

		if(!(P1IN & PUSHBTN))
		{
			return(ON);
		}
	}

	return(OFF);
}

/*
 *  Read shutdwon feedback
 *  Return 1 if the pushbutton is pressed, 0 otherwise
 */ 
unsigned char readShutdownFeedback()
{
	unsigned short index;
	
	if(!(P1IN & SHTDIN))
	{
		for(index=0; index < 2000; index++);

		if(!(P1IN & SHTDIN))
		{
			return(ON);
		}
	}

	return(OFF);
}

/*
 *  Handle LED state
 */ 
void LEDPower()
{
   switch(LEDStatus)
	{
		case ON:
			RPOF_PORT |= LED;
			break;

		case OFF:
			RPOF_PORT &= ~LED;
			break;
		
		case FLASH:
			if(TimeFlash == 0)
			{
				if(RPOF_PORT & LED)
					RPOF_PORT &= ~LED;
				else	
					RPOF_PORT |= LED;
				
				TimeFlash = FLASHTIME;
			}
			break;
		
		default:
			LEDStatus = OFF;
			break;
	}
}

/*
 *  Control the Relay
 */ 
void Relay()
{
   switch(RelayStatus)
	{
		case ON:
			RPOF_PORT |= RELAY;
			RPOF_PORT |= LED_DBG;	/* Turn ON debug LED */
			break;

		case OFF:
			RPOF_PORT &= ~RELAY;
			RPOF_PORT &= ~LED_DBG;	/* Turn ON debug LED */
			break;
			
		default:
			RelayStatus = OFF;
			break;
	}
}

/*
 *  Timer interrupt - every .01 s (10 ms)
 */ 
interrupt(TIMERA0_VECTOR) TIMERA0_ISR(void)
{
	/* LED flash timer */
   if(TimeFlash)
		TimeFlash--;

	/* Turning OFF everything timer */
	
	if(OFFTimer)
	{
		RPOF_PORT ^= BIT7;		/* Debug */
		
		if(SafetyTimer)
			SafetyTimer--;
		else
		{
			SafetyTimer = SAFETIME;
			if(OFFTimer)
				OFFTimer--;
		}
	}
}



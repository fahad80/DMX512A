/* 
DMX512 Communication Protocol

Spring 2014

All rights @ 
Fahad Mirza (fahad.mirza34@mavs.uta.edu)

Course Project
Embedded System Controller - EE5314
Dr. Jason Losh
Department of Electrical Engineering
University of Texas at Arlington

DEVICE code

-----------------------------------------------------------------------------
Notes             
-----------------------------------------------------------------------------
File Name	: 'main.c'
Created		: 4th March, 2014
Revised		: 28th April, 2014
Version		: 1.0
Target uC	: 33FJ128MC802
Clock Source: 8 MHz primary oscillator set in configuration bits
Clock Rate	: 80 MHz using prediv=2, plldiv=40, postdiv=2
Devices used: UART

-----------------------------------------------------------------------------
Objective             
-----------------------------------------------------------------------------
Get instruction(RS485) from CONTROLLER, and execute.

-----------------------------------------------------------------------------
Hardware description          
-----------------------------------------------------------------------------
Green LED: Anode connected through 100ohm resistor to RB4 (pin 11), cathode 
grounded.

Red LED: anode connected through 100ohm resistor to RB5 (pin 14), cathode 
grounded.

Device Address: RB3,RA4,RB9-RB15 are connected with 9 pin Dip switch. 

*/


#include <p33FJ128MC802.h>
#include <stdio.h>
#include "uart2.h"


#define dmxWrOn LATBbits.LATB8			 	// RS485 Read/Write Enable Pin RB8 (pin17)
#define BAUD_250K 9							// UART2-->(40M/16/250K)-1
#define BAUD_96153 25						// UART2-->(40M/16/96513)-1 
											// For Break & MAB. This baudrate will give 20us MAB and 92us Break 


//-------- Delay functions --------//
extern void wait_us(unsigned int n);
extern void wait_ms(unsigned int n);


// Global Variables
unsigned int devAdd;			// Device Address Variable


// Debug Variable
unsigned char dataOf512;

// Timer1 Interrupt Variables
volatile int redTimeout,grnTimeout,noDataTimeout;		// LED blink and Invalid DMX timeout variables.


//-----------------------------------------------------------------------------
// Interrupt Subroutines                
//-----------------------------------------------------------------------------

// For TIMER1
void __attribute__((interrupt, no_auto_psv)) _T1Interrupt (void)
{
	TMR1 = 0;
	if(redTimeout>0)
	{
		redTimeout--;
		if(redTimeout <= 0) LATBbits.LATB5 = 0;
	}

	if(grnTimeout>0)
	{
		grnTimeout--;
		if(grnTimeout <= 0) LATBbits.LATB4 = 1;
	}

	if(noDataTimeout>0)
	{
		noDataTimeout--;
		if(noDataTimeout <= 0) LATBbits.LATB4 = 0;
	}
	// clear IF
	IFS0bits.T1IF = 0;
 }


//-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
// Subroutines                
//-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-


//*********************//
// Initialize Hardware //
//*********************//
void init_hw()
{
  PLLFBDbits.PLLDIV = 38;                   // PLL feedback divider = 40;
  CLKDIVbits.PLLPRE = 0;                    // PLL pre divider = 2
  CLKDIVbits.PLLPOST = 0;                   // PLL post divider = 2

  LATBbits.LATB4 = 0;                       // Write 0 into RB4 output latche
  LATBbits.LATB5 = 0;						// Write 0 into RB5 output latche
  TRISBbits.TRISB4 = 0;                     // Make green led pin an output
  TRISBbits.TRISB5 = 0;                     // Make red led pin an output

  AD1PCFGLbits.PCFG5 = 1;					// Change RB3 to digital pin					
  TRISB |= 0xFE08;							// RB3,RB9-RB15 --> o/p
  TRISAbits.TRISA4 = 1;						// RA4 --> o/p
  
  CNPU1 |= 0xF881;							// Enable pull-up for DIP Switch 
  CNPU2bits.CN16PUE = 1;
  CNPU2bits.CN21PUE = 1;		 

  LATBbits.LATB8 = 0;						 // Write 0 into RB8 output latche
  TRISBbits.TRISB8 = 0;                      // Make DEn pin an output
  RPINR19bits.U2RXR = 7;                     // Assign U2RX to RP7
  RPOR3bits.RP6R = 5;                        // Assign U2TX to RP6

  RPOR1bits.RP2R = 18;                       // connect OC1 to RP2 (PWM)
}


void pwm_init()
{
  // Clock timer 2 with internal 40 MHz clock
  // and set period to 1.63ms
  T2CONbits.TCS = 0;	// Select Fcy
  T2CONbits.TCKPS = 3;	// Prescaler 256
  T2CONbits.TON = 1;
  PR2 = 255;
  
  OC1CON = 0;			// turn-off OC1 to make sure changes can be applied
  OC1R = 0;				// set first cycle d.c.
  OC1RS = 0;			// set ongoing d.c. to 0
  OC1CONbits.OCTSEL = 0;// Select Timer 2 as source
  OC1CONbits.OCM = 6;	// Set OC1 to PWM mode
}


void pwm_setdc(unsigned int dc)
{
  OC1RS = dc;
}

//*******************//
// Initialize Timer1 //
//*******************//
void timer1_init(unsigned int period)
{
  TMR1 = 0;				// Clear counter
  // Clock timer 1 with internal 40/64 MHz clock 
  T1CONbits.TCS = 0;	// Select Fcy
  T1CONbits.TCKPS = 2;	// Prescaler 64
  T1CONbits.TON = 1;
  PR1 = period;			
  
  IFS0bits.T1IF = 0;	// Clear Flag
  IEC0bits.T1IE = 1;	// Enable timer 1 interrupts
}


//****************************************************//
// Read Device Address 								  //
//----------------------------------------------------//
// 9bit address. From RB15 to RB9,then RA4, then RB3. //
//****************************************************//
void readDevAdd()
{
   devAdd = (PORTB & 0xFE00)>>7;			// Read frm RB15 to RB9
   devAdd |= (PORTAbits.RA4)<<1;			// Read RA4
   devAdd |= PORTBbits.RB3;					// Read RB3
   devAdd = (~devAdd)&0x01FF;				// Active Low. So take compliment.
   devAdd += 1;
}


//*******************************//
// DMX Break generation function //
//*******************************//
void brkFunc()
{
	// Initiating BREAK and MAB
	while(!U2STAbits.TRMT);			// Wait till Tx buffer empty		
	dmxWrOn = 1;					// DMX Write Enable
	U2BRG = BAUD_96153;				// Slow the Baud Rate 
	uart2_putc(0);					// Sending Break and MAB
	while(!U2STAbits.TRMT);			// Wait till Brk & MAB transmitted
	// Break and MAB end
			
	U2BRG = BAUD_250K;				// Return to default speed.
}


//**********************************//
// Retrive the poll data from RS485 //
//**********************************//
int retriveData()
{
	unsigned int index = 1;		// Index Variable for DMX buffer
   	unsigned char data,temp;	

	while(index<513)			// Read all 512 data
	{
		temp = uart2_getc();
		if(index == devAdd)		// Is the data is for this device?
		{
			data = temp;	
		}
//		else if(index == 512)		// Debugging. Device will have two address. Switch address and fixed 512
//		{
//			dataOf512 = temp;	
//		}
		index++;
	}

	return data;
}



//----------------------------------------------------------------------------
// MAIN starts here
//----------------------------------------------------------------------------

int main()
{ 
   // Variable Declarations
   unsigned char temp;
   unsigned char dmxData;			// DMX Data
   unsigned char pollData;			// Poll data
   unsigned int dmxRdIndex;			// Keep track of dmxData
   int brkFlag = 0;					// Indication of Break


   // Initializations...
   init_hw();                 		// Initialize hardware
   pwm_init();	
   uart2_init(BAUD_250K);			// Configure uart2
   timer1_init(625);				// (40M/64)*1m = 625
   
   dmxWrOn = 0; 					// DMX Read On

   LATBbits.LATB4 = 1;         		// Blink green LED for 500ms, Step 1
   wait_ms(500);
   LATBbits.LATB4 = 0;


   while(1)
   {
		readDevAdd();							// Read Device Address

		if(U2STAbits.URXDA)						// Is there any data in USART2
		{
			if(U2STAbits.FERR)					// Is it with frame error?
			{	
				if(uart2_getc() == 0)			// Is it Break?
				{
					temp = uart2_getc();
					if(temp == 0)				// Is start code is zero?
					{
						brkFlag = 1;			// Got the Break, set the flag.
						dmxRdIndex = 1;			// Initialize the Index
						noDataTimeout = 1000;	// If next break isn't within 1s, then turn off the Grn LED in Timer.
						if(grnTimeout <= 0)		// Solid LED, coz we have valid dmx data (set LED only when grnTimeout is zero)
							LATBbits.LATB4 = 1;
					}
					else if(temp == 0xF0)		// Is it POLL code?
					{
						pollData = retriveData();
						
						if(pollData)
						{
							dmxWrOn = 1; 				// DMX Write On
							wait_us(1);					// Give time to properly convert from read to write mode
							brkFunc();					// Send Break
							redTimeout = 250;			// Set RED LED, indicate break is sent
							LATBbits.LATB5 = 1;
							dmxWrOn = 0; 				// DMX Read On
						}
					}
				}
			}
			else if(brkFlag)					// Do we have a valid Break?
			{	
				temp = uart2_getc();			// Read the data
				if(dmxRdIndex == devAdd)		// Is the data for the device?
				{	
					if(temp != dmxData)
					{
						grnTimeout = 250;
						LATBbits.LATB4 = 0;
					}
					dmxData = temp;				// Save the data
					pwm_setdc(temp);
					brkFlag = 0;				// Reset the Break flag i.e. again wait for break
					dmxRdIndex = 0;				// Reset the DMX data index. Not Necessary!
				}

				if (++dmxRdIndex>=513) 			// Avoid overflow of the index
					dmxRdIndex = 0;
			}
			else
			{	
				uart2_getc();					// To avoid overrun keep reading the data.
			}
		}
   }
   
   return 0;
}

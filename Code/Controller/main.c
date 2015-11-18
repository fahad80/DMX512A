/*! \file main.c \brief DMX512 Communication. */
//*****************************************************************************
/*
DMX512 Communication

All rights reserved @ 
Fahad Mirza (fahad.mirza34@mavs.uta.edu)


Course Project (Spring 2014)
Embedded System Controller - EE5314
Dr. Jason Losh
Department of Electrical Engineering
University of Texas at Arlington


Controller Code

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

------------------------------------------------------------------------------------
Objective             
------------------------------------------------------------------------------------
CONTROLLER Code:
Get command(string) from PC (RS-232), parse it and send instruction through RS-485.

------------------------------------------------------------------------------------
Hardware description          
------------------------------------------------------------------------------------
Green LED: Anode connected through 100ohm resistor to RB4 (pin 11), cathode grounded.

Red LED: Anode connected through 100ohm resistor to RB5 (pin 14), cathode grounded.

Push button: Connected between RB15 (pin 26) and ground.

SP3232E RS-232 Interface
  T1IN connected to RP10 (pin 21)
  R1OUT connected to RP11 (pin 22)

*/

#include <p33FJ128MC802.h>
#include <stdio.h>
#include "uart1.h"
#include "uart2.h"
#include "main.h"


#define dmxWrOn LATBbits.LATB8		// RS485 Read/Write Enable Pin RB8 (pin17)


#define pollCode 0xF0				// Start code for POLL
#define dataCode 0x00				// Start code for normal data


//-------- Delay functions --------//
extern void wait_us(unsigned int n); 
extern void wait_ms(unsigned int n);


//-----------------------------------------------------------------------------
// Interrupt Subroutines                
//-----------------------------------------------------------------------------

// For UART1 Tx
void __attribute__((interrupt, no_auto_psv)) _U1TXInterrupt(void)
{
	if(rd_index != wr_index)			// Is there any data available to send?
	{
		U1TXREG = txBuf[rd_index++];	// Put data into Tx buffer and increse index by 1
		FULL = 0;						// if the buffer was FULL, now at least one space available to store new byte in the Txbuff.
	}

	IFS0bits.U1TXIF = 0;				// Clear the flag
}


// For TIMER1
void __attribute__((interrupt, no_auto_psv)) _T1Interrupt (void)
{
	TMR1 = 0;						// Clear the Counter register
	if(redTimeout>0)				// Check time for red LED
	{
		redTimeout--;
		if(redTimeout <= 0) LATBbits.LATB5 = 0;
	}

	if(grnTimeout>0)				// Check time for green LED
	{
		grnTimeout--;
		if(grnTimeout <= 0) LATBbits.LATB4 = 0;
	}
	
	IFS0bits.T1IF = 0;				// clear IF
 }


//-----------------------------------------------------------------------------
// Subroutines                
//-----------------------------------------------------------------------------

//*******************************//
// DMX Break generation function //
//*******************************//
void brkFunc(unsigned char startCode)
{
	/*-- Initiating BREAK and MAB --*/
	while(!U2STAbits.TRMT || !U2STAbits.RIDLE);			// Wait till Tx buffer is empty and till Rx is idle
	
	dmxWrOn = 1;					// DMX Write Enable
	U2BRG = BAUD_96153;				// Slow the Baud Rate 
	uart2_putc(0);					// Sending Break and MAB
	while(!U2STAbits.TRMT);			// Wait till Brk & MAB transmitted
	/*---- Break and MAB end ----*/
			

	U2BRG = BAUD_250K;				// Return to default speed.
	uart2_putc(startCode);			// Send Start Code 
}


//****************************//
// POLL data sending function //
//****************************//
int poll(int min, int max)
{
	int i,j;						// 'for loop' variables
	char pollData[512];				// Poll Buffer


	for(i=1;i<513;i++)				// Prepare poll data. Set '1' from min to max, '0' for the rest
	{
		if(i>=min && i<=max) 
			pollData[i-1] = 1;		// pollData start from 0 to 511
		else 
			pollData[i-1] = 0;
	}
	
	if(pollDevAddrIndex>0)			// Did we already find any device address on the bus?
	{
		for(i=1,j=0;i<513;i++) 		// If so, check and set those address's poll data as 0
		{
			if(i == pollDevAddr[j])	
			{
				pollData[i-1] = 0;
				j++;
				if(j>= pollDevAddrIndex) break; // If we already clear all 'Found' devices address, no need to check further
			}
		}	
	}

	brkFunc(pollCode);				// Send Break and MAB, with Start Code 0xF0
	
	for(i=0;i<512;i++)
	{
		uart2_putc(pollData[i]);	// Send all 512 data
	}

	while(!U2STAbits.TRMT);			// Wait till data transmit ends

	dmxWrOn = 0;					// Enable read mode

	for(i=0;i<60;i++)				// To detect a data zero with a frame error requires 44uS, we will check for data till 60uS
	{
		if(U2STAbits.URXDA)					// Is there any data?
		{
			if(U2STAbits.FERR)				// Is it with frame error?
			{	
				if(uart2_getc() == 0)		// Is it Break?
				{
					redTimeout = 250;		// Set RED LED, indicate valid break receive
					LATBbits.LATB5 = 1;
					return 1;				// Got a response, return 1
				}
			}
			else
			{
				uart2_getc();				// To avoid overrun keep reading the data.
			}
		}
		wait_us(1);
	}
	return 0;								// No response found, return 0
}


//**********************************************************//
// Find all devices addresses in the bus //
//**********************************************************//
void findDevice()
{
	int min=1,max,temp;
	pollDevAddrIndex = 0; 		// Initialize number of devices variable

	while(poll(1,512))			// Is there any device at all?
	{
		max = 512;
		while(1)
		{
			if(poll(min,max))
			{
				if(min == max)	// We found one device address
				{
					pollDevAddr[pollDevAddrIndex++] = min;
					break;		// return back to '1 to 512' range and do everything again.
				}
				max = (min+max-1)>>1;	// Devide the range into half
			}
			else						// Didn't get response from left tree, so set min and max value for right tree
			{
				temp = min;			
				min = max+1;
				max = (max*2)+1-temp;
			}
		}
	}
	// Found all the available device (if there is any) in the bus
}


//----------------------------------------------------------------------------
// MAIN starts here
//----------------------------------------------------------------------------

int main()
{
   	int dmxIndex;				// Index Variable for RS485 buffer
	char buffer[6];				// Variable for itoa

   	init_hw();					// Initialize hardware
   	uart1_init(BAUD_19200);		// Configure uart1
	uart2_init(BAUD_96153);		// Configure uart2
	timer1_init(625);			// (625*64)/40M = 1ms
    
	dmxWrOn = 1; 				// DMX Write On
	clrDmxData();				// Initialize DMX buffer with zero
	
   	LATBbits.LATB4 = 1;			// Blink green LED for 500ms, Step 1
   	wait_ms(500);
   	LATBbits.LATB4 = 0;

	send_string(welcome);		// Welcome String

   	while(1)
   	{
		if(dmxOn)							// Is DMX on?
		{
			brkFunc(dataCode);				// Send Break,MAB with start code 0x00
			dmxIndex = 1;					// Initialize DMX buffer index
			
			while(dmxIndex<=maxDmxAddr)		// Send data till max addr
			{
				uart2_putc(dmxData[dmxIndex++]);
				
				if(U1STAbits.URXDA)			// Cheack if there is any data in UART1.
				{
					processCmd();	
				}
			}
			
			if(pollFlag == 1)				
			{
				pollFlag = 0;
				findDevice();					// Find available device addresses in the bus

				if(pollDevAddrIndex>0)			// Did we find any device?
				{
					pollDevAddrIndex--;			// Decrement one
					while(pollDevAddrIndex>=0)	// Print all the address in UART1
					{
						itoa(buffer,pollDevAddr[pollDevAddrIndex--],10);
						send_string("\r\n");
						send_string(buffer);
						send_string(" ");
					}
				}
				else send_string(noDev);		// No device found on the bus
				send_string(ready);
			}
		}
		else if(U1STAbits.URXDA)			// DMX is off. So check if there is any data in UART1
		{
			processCmd();
		}
   	}

   	return 0;
}



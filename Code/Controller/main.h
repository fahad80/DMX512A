/*! \file main.h \brief DMX512 Communication. */
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

-----------------------------------------------------------------------------
Notes             
-----------------------------------------------------------------------------
File Name	: 'main.h'
Created		: 14th March, 2014
Revised		: 30th April, 2014
Version		: 1.0
Target uC	: 33FJ128MC802
Clock Source: 8 MHz primary oscillator set in configuration bits
Clock Rate	: 80 MHz using prediv=2, plldiv=40, postdiv=2
Devices used: UART

------------------------------------------------------------------------------------
Objective             
------------------------------------------------------------------------------------
CONTROLLER Code:
Get command(string) from PC (RS-232), parse it and send instruction through 
RS-485.

------------------------------------------------------------------------------------
Hardware description          
------------------------------------------------------------------------------------
See 'main.c'

*/

#include <p33FJ128MC802.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "uart1.h"
#include "uart2.h"



#define BAUD_19200 129                       // brg for low-speed, 40 MHz clock // UART1-->round((40000000/16/19200)-1)
#define BAUD_250K 9							 // UART2-->(40M/16/250K)-1 
#define BAUD_96153 25						 // UART2-->(40M/16/96513)-1 (For Break and MAB) 
											 // This baudrate will give 20us MAB and 93us Break 


// Constants Array
const char errMsg[] = "\r\nError. Type 'help'.\r\n";
const char ready[] = "\r\nReady.\r\n";
const char noDev[] = "\r\nNo Device Found.\r\n";
const char welcome[] = "\r\nWelcome.\r\nFor cmd type 'help'.\r\n";
const char help[] = "\r\nCmds are case insensetive.\r\nAdr:1 to 512; data:0 to 255\r\n---------------------------\r\nset Adr data\r\nget Adr\r\nmax Adr\r\non\r\noff\r\npoll\r\nclear\r\n";



// Global Variables
unsigned char dmxData[513];			// RS485 Buffer
int pollDevAddr[10];				// Available Device Address Storage; Right now assuming there can be max 10 device in the bus. 
int pollDevAddrIndex = 0;
unsigned int maxDmxAddr = 512;		// Max Data slot for RS485
unsigned char dmxOn = 1;			// RS485 On/Off

int pollFlag=0;						// POLL commands flag. Execute outside of the processCMD function.

char type[5],inStr[21];				// User string (from UART1) parsing variables.
unsigned int pos[5];				
unsigned int field_count;

// UART1 Tx interrupt variables
unsigned char txBuf[256];			// Ring Buffer
unsigned char wr_index=0;			// Tx Buffer write & read index
unsigned char rd_index=0;			// unsigned char so after 255 they automatically return to zero.
int FULL=0;							// Buffer full flag


// Timer1 Interrupt Variables
int redTimeout=0,grnTimeout=0;		// LED blink variables.


//-----------------------------------------------------------------------------
// Subroutines                
//-----------------------------------------------------------------------------

//*********************//
// Initialize Hardware //
//*********************//
void init_hw()
{
  PLLFBDbits.PLLDIV = 38;                    // pll feedback divider = 40;
  CLKDIVbits.PLLPRE = 0;                     // pll pre divider = 2
  CLKDIVbits.PLLPOST = 0;                    // pll post divider = 2

  LATBbits.LATB4 = 0;                        // write 0 into RB4 output latche
  LATBbits.LATB5 = 0;						 // write 0 into RB5 output latche
  TRISBbits.TRISB4 = 0;                      // make green led pin an output
  TRISBbits.TRISB5 = 0;                      // make red led pin an output
  CNPU1bits.CN11PUE = 1;                     // enable pull-up for push button
  RPINR18bits.U1RXR = 11;                    // assign U1RX to RP11
  RPOR5bits.RP10R = 3;                       // assign U1TX to RP10

  LATBbits.LATB8 = 0;						 // write 0 into RB8 output latche
  TRISBbits.TRISB8 = 0;                      // make DEn pin an output
  RPINR19bits.U2RXR = 7;                     // assign U2RX to RP7
  RPOR3bits.RP6R = 5;                        // assign U2TX to RP6
}


void timer1_init(unsigned int period)
{
  TMR1 = 0;				// Clear counter
  // Clock timer 1 with internal 40/64 MHz clock 
  T1CONbits.TCS = 0;	// Select Fcy
  T1CONbits.TCKPS = 2;	// Prescaler 64
  T1CONbits.TON = 1;
  PR1 = period;
  IFS0bits.T1IF = 0;	// Clear the flag
  IEC0bits.T1IE = 1;	// Enable timer 1 interrupts
}


//********************************************//
// Put data in 'txBuf' for UART1 Tx interrupt //
//********************************************//
void send_string(const char str[])
{
	int i=0;
	
	while (str[i] != 0 && !FULL)			// Write to buffer if it isn't FULL
	{
		txBuf[wr_index++] = str[i++];
		if((wr_index+1)%256 == rd_index)	// Is next byte going to overwrite the buffer?
			FULL = 1;						// Then SET FULL
		else 
			FULL = 0;
	}
	if(U1STAbits.UTXBF == 0) 				// Is TX FIFO is completely empty?
		U1TXREG = txBuf[rd_index++];		// Then at least write one byte to FIFO to start interrupt
}


//*************************//
// Get character from UART //
//*************************//
int getInputChar()
{
   unsigned char temp;				
   static unsigned int count = 0;	// Limit user input within 20 character
   
   temp = uart1_getc();				// Get Character
   if((temp == 8) && (count>0))		// Is it 'backspace'?
   {
      count--;
   }
   else if(temp == '\r')			// Is it CR?
   {
      inStr[count] = NULL;			// Complete the buffer
	  count = 0;
	  return 1;
   }
   else if(temp>=32 && temp<=126)	// Is it a 'Printable character'?
   {
      inStr[count++]=tolower(temp);	// Store with lower case
      if(count==20)					// Is buffer full?
	  {
		 inStr[count] = NULL;
		 count = 0;
		 return 1;
   	  }   
   }
   return 0;						// Return 0 if it isn't CR
}


//***********************//
// Parse the user string //
//***********************//
unsigned int parseStr()
{
   unsigned int i;						// 'for' loop variables
   unsigned int j=0,errorFlag=0;		// invalid cmd flag
   unsigned char last_type = 'd';
   
   field_count = 0;						// Initialize the global variable

   for(i=0;inStr[i] != NULL && i<=20; i++)	// parse till string is null or till 20 character
   {
      if(isalnum(inStr[i]))				// Is it Alpahanumeric?
      {
	     if(isalpha(inStr[i]))			// Is it Alphabet?
		 {
		    if(last_type == 'd')		// d -> a transition. Record the position
			{
			   pos[j] = i;
			   type[j++] = 'a';
			   last_type = 'a';
			   field_count++;
			}
			else if(last_type == 'n')	// n -> a tansition. Error.
			{
			   errorFlag = 1;
			   break;
			} 
		 }
         else if(isdigit(inStr[i]))		// Is it Number?
		 {
		    if(last_type == 'd')		// d -> n transition. Record the position.
			{
			   pos[j] = i;
			   type[j++] = 'n';
			   last_type = 'n';
			   field_count++;
			}
			else if(last_type == 'a')	// a -> n transition. Error.
			{
			   errorFlag = 1;
			   break;
			}
		 }
	  }
	  else					// If it is not Alphanumeric then it is delimiter. Replace with NULL.
	  {
		 inStr[i] = NULL;
	     last_type = 'd';
	  }
   }	
   return errorFlag;			// Return 1 if there is invalid sequence. Otherwise return zero.
}


//*********************//
// Check Valid Command //
//*********************//
int isCmd(char strVerb[], int count) // Check user string with predefined command. Return 1 if it match. Zero otherwise.
{
   return ((strcmp(strVerb, &inStr[pos[0]])==0) && (count == field_count));
}


//********************//
// atoi for the inStr //
//********************//
int getArgNum(int index)
{
   return (atoi(&inStr[pos[index]]));	//Change 'addr' and 'value' from ascii to integer
}


//******************//
// Clear DMX Buffer //
//******************//
void clrDmxData()
{
	int i;
	for(i=0;i<513;i++)
	{
		dmxData[i] = 0;
	}
}

//**************************************//
// Read user data & process accordingly //
//**************************************//
void processCmd()
{
	int addr, data, invalidCmd=0;	
	char buffer[6];


	if(getInputChar())				// Get character form UART1. If it is CR then go to next step.
	{
		if(!parseStr())				// Start parsing. If there isn't any error, execute next step.
	  	{
			if(isCmd("set",3))		// Is it 'SET'?
			{
				if(type[1]=='n' && type[2]=='n') 	// Are both of the parameters number?
				{
					addr = getArgNum(1);
					data = getArgNum(2);
					if((addr>=1 && addr<=512) && (data>=0 && data<=255)) // Check if the parameters are within range
						dmxData[addr] = data;
					else 
						invalidCmd = 1;
				}
				else invalidCmd = 1;				// Both parameters arent number. Error.
			}
			else if(isCmd("get",2))					// Is it 'GET' cmd?
			{
				if(type[1]=='n')					// Is the parameter a number?
				{
				   	addr = getArgNum(1);
					if((addr>=1 && addr<=512))		// Check if the parameter is within range
					{
						itoa(buffer,dmxData[addr],10);
						send_string("\r\n");
						send_string(buffer);
					}
					else invalidCmd = 1;
				}
				else invalidCmd = 1;
			}
			else if(isCmd("max",2))					// Is it a 'MAX' cmd?
			{
				if(type[1]=='n')
				{
					addr = getArgNum(1);
					if((addr>=1 && addr<=512)) 		// Check if the parameter is within range
						maxDmxAddr = addr;
					else 
						invalidCmd = 1;
				}
				else invalidCmd = 1;
			}
			else if(isCmd("clear",1))				// Is it a 'CLEAR' cmd?
			{
				clrDmxData();
			}
			else if(isCmd("on",1))					// Is it a 'ON' cmd?
			{
				dmxOn = 1;							// Turn ON the DMX transmission.
			}
			else if(isCmd("off",1))					// Is it a 'OFF' cmd?
			{
				dmxOn = 0;							// Turn OFF the DMX transmission.
			}
			else if(isCmd("poll",1))
			{
				pollFlag = 1;						// Set the pollFlag, and execute after the end of ongoing DMX transmission. 
			}
			else if(isCmd("help",1))
			{
				send_string(help);					// Send "HELP" string.
			}
			else 
				invalidCmd = 1;						// If nothing matches, certainly it is a invalid CMD
			
		}
		else
		{
			invalidCmd = 1;							// parseStr return error!
		}

		if(invalidCmd)								// If the cmd is invalid send error msg. 
		{
			send_string(errMsg);
			invalidCmd = 0;
		}
		else if (!invalidCmd)			// Don't send READY if it is POLL
		{
			grnTimeout = 250;
			LATBbits.LATB4 = 1;
			if(!pollFlag)
				send_string(ready);
		}
	}
}



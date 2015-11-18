/*! \file main.c \brief DMX512 Communication. */
//*****************************************************************************
/*
DMX512 Communication

All rights reserved @ 
Fahad Mirza (fahad.mirza34@mavs.uta.edu)
Souvik Dubey (souvik.dubey@mavs.uta.edu)


Course Project (Spring 2014)
Embedded System Controller - EE5314
Dr. Jason Losh
Department of Electrical Engineering
University of Texas at Arlington

-----------------------------------------------------------------------------
Notes             
-----------------------------------------------------------------------------
File Name	: 'main.c'
Created		: 14th March, 2014
Revised		: 
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
Green LED: Anode connected through 100ohm resistor to RB4 (pin 11), cathode grounded.

Red LED: Anode connected through 100ohm resistor to RB5 (pin 14), cathode grounded.

Push button: Connected between RB15 (pin 26) and ground.

SP3232E RS-232 Interface
  T1IN connected to RP10 (pin 21)
  R1OUT connected to RP11 (pin 22)

*/

#include <p33FJ32MC202.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "uart1.h"


#define dmxTranOn LATBbits.LATB8

#define BAUD_19200 129                       // brg for low-speed, 40 MHz clock // UART1-->round((40000000/16/19200)-1)
#define BAUD_250K 9							 // UART2-->(40M/16/250K)-1
#define BAUD_125K 19						 // UART2-->(40M/16/125K)-1 
											 // For Break & MAB. This baudrate will give 16us MAB and 104us Break 

// Constants
const char errMsg[] = "Error. Type 'help'.\r\n";
const char ready[] = "Ready.\r\n";

// Global Variables
unsigned char dmxData[513];
unsigned int maxDmxAddr = 512;
unsigned char dmxOn = 1;
char type[5],inStr[21];
unsigned int pos[5];
unsigned int field_count;

//-----------------------------------
extern void wait_us(unsigned int n);
extern void wait_ms(unsigned int n);
extern void change_osc(void);


//-----------------------------------------------------------------------------
// Subroutines                
//-----------------------------------------------------------------------------


//*********************//
// Initialize Hardware //
//*********************//
void init_hw()
{
  change_osc();


  //PLLFBDbits.PLLDIV = 38;                    // pll feedback divider = 40;
  //CLKDIVbits.PLLPRE = 0;                     // pll pre divider = 2
  //CLKDIVbits.PLLPOST = 0;                    // pll post divider = 2

  LATBbits.LATB4 = 0;                        // write 0 into RB4 output latche
  LATBbits.LATB5 = 0;						 // write 0 into RB5 output latche
  TRISBbits.TRISB4 = 0;                      // make green led pin an output
  TRISBbits.TRISB5 = 0;                      // make red led pin an output
  CNPU1bits.CN11PUE = 1;                     // enable pull-up for push button
  RPINR18bits.U1RXR = 11;                    // assign U1RX to RP11
  RPOR5bits.RP10R = 3;                       // assign U1TX to RP10

  LATBbits.LATB8 = 0;						 // write 0 into RB8 output latche
  TRISBbits.TRISB8 = 0;                      // make DEn pin an output
  //RPINR19bits.U2RXR = 7;                     // assign U2RX to RP7
  //RPOR3bits.RP6R = 5;                        // assign U2TX to RP6
}


//******************************//
// Get string commang from UART //
//******************************//
int getInputChar()
{
   unsigned char temp;
   static unsigned int count = 0;
   
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
      inStr[count++] = temp;
      if(count==20)					// Is buffer full?
	  {
		 inStr[count] = NULL;
		 count = 0;
		 return 1;
   	  }   
   }
   return 0;
}


//******************//
// Parse the string //
//******************//
unsigned int parseStr()
{
   unsigned int i;
   unsigned int j=0,errorFlag=0;
   unsigned char last_type = 'd';
   
   field_count = 0;

   for(i=0;inStr[i] != NULL && i<=20; i++)
   {
      if(isalnum(inStr[i]))
      {
	     if(isalpha(inStr[i]))
		 {
		    if(last_type == 'd')
			{
			   pos[j] = i;
			   type[j++] = 'a';
			   last_type = 'a';
			   field_count++;
			}
			else if(last_type == 'n')
			{
			   errorFlag = 1;
			   break;
			} 
		 }
         else if(isdigit(inStr[i]))
		 {
		    if(last_type == 'd')
			{
			   pos[j] = i;
			   type[j++] = 'n';
			   last_type = 'n';
			   field_count++;
			}
			else if(last_type == 'a')
			{
			   errorFlag = 1;
			   break;
			}
		 }
	  }
	  else
	  {
		 inStr[i] = NULL;
	     last_type = 'd';
	  }
   }	
   return errorFlag;		
}


//*********************//
// Check Valid Command //
//*********************//
int isCmd(char strVerb[], int count)
{
   return ((strcmp(strVerb, &inStr[pos[0]])==0) && (count == field_count));
}


//********************//
// atoi for the inStr //
//********************//
int getArgNum(int index)
{
   int value=0;

   value = atoi(&inStr[pos[index]]);
   return value;
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
	int addr, data, invalidCmd;
	char buffer[6];

	if(getInputChar())
	{
		if(!parseStr())
	  	{
			if(isCmd("set",3))
			{
				if(type[1]=='n' && type[2]=='n')
				{
					addr = getArgNum(1);
					data = getArgNum(2);
					dmxData[addr] = data;
				   //serial_puts(&inStr[pos[1]]);
				   //serial_puts(", ");
				   //serial_puts(&inStr[pos[2]]);
				   //serial_puts("\r\n");
				}
				else invalidCmd = 1;
			}
			else if(isCmd("get",2))
			{
				if(type[1]=='n')
				{
				   	addr = getArgNum(1);
					itoa(buffer,dmxData[addr],10);
					uart1_puts(buffer);
				   	//serial_puts(&inStr[pos[1]]);
				   	//serial_puts("\r\n");
				}
				else invalidCmd = 1;
			}
			else if(isCmd("max",2))
			{
				if(type[1]=='n')
				{
					maxDmxAddr = getArgNum(1);
				   	//serial_puts(&inStr[pos[1]]);
				   	//serial_puts("\r\n");
				}
				else invalidCmd = 1;
			}
			else if(isCmd("clear",1))
			{
				clrDmxData();
			}
			else if(isCmd("on",1))
			{
				dmxOn = 1;
			}
			else if(isCmd("off",1))
			{
				dmxOn = 0;
			}
			else if(isCmd("poll",1))
			{
				// To Do: Code here...
			}
			else invalidCmd = 1;
			
		}
		else
		{
			invalidCmd = 1;
		}

		if(invalidCmd == 1)
		{
			uart1_puts(errMsg);
			invalidCmd = 0;
		}
		else
		{
			uart1_puts(ready);
		}
	}
}



//----------------------------------------------------------------------------
// MAIN starts here
//----------------------------------------------------------------------------

int main()
{
   	int dmxIndex;				// Variable for RS485
   

   	init_hw();					// Initialize hardware
   	uart1_init(BAUD_19200);		// Configure uart1
	//uart2_init(BAUD_125K);		// Configure uart2
    
	dmxTranOn = 1; 				// Transmit DMX Data
	clrDmxData();				// Initialize DMX buffer with zero
	
   	LATBbits.LATB4 = 1;			// Blink green LED for 500ms
   	wait_ms(500);
   	LATBbits.LATB4 = 0;
	uart1_puts(ready);

   	while(1)
   	{
		if(dmxOn)
		{
			//U1BRG = BAUD_125K;
			//U1STAbits.UTXBRK = 1;
			//while(U1STAbits.UTXBRK);
			//U1BRG = BAUD_250K;
			//dmxIndex = 0;
			
			//while(dmxIndex<513)
			//{
				//uart1_putc(dmxData[dmxIndex++]);
				
				if(U1STAbits.URXDA)
				{
					processCmd();	
				}
			//}
		}
		//else if(U1STAbits.URXDA)
		//{
		//	processCmd();
		//}
   	}

   	return 0;
}

/*! \file uart2.c \brief UART function library. */
//*****************************************************************************
// Main Author: Dr. Jason Losh
// Modified by Fahad Mirza
//-----------------------------------------------------------------------------
// Objectives and notes             
//-----------------------------------------------------------------------------
// File Name	: 'uart2.c'
// Title		: UART functions
// Created		: 5th April, 2014
// Revised		: 28th April, 2014
// Version		: 1.0

// Target uC:       33FJ128MC802
// Clock Source:    8 MHz primary oscillator set in configuration bits
// Clock Rate:      80 MHz using prediv=2, plldiv=40, postdiv=2
// Devices used:    UART
//*****************************************************************************



//-----------------------------------------------------------------------------
// Device includes and assembler directives             
//-----------------------------------------------------------------------------
#include <p33FJ128MC802.h>
#include <stdio.h>
#include <string.h>
#include "uart2.h"

//-----------------------------------------------------------------------------
// Subroutines                
//-----------------------------------------------------------------------------

// Initializes UART2 with desired Baud rate
void uart2_init(int baud_rate)
{
  // set baud rate
  U2BRG = baud_rate;
  // enable uarts, 8N1, low speed brg
  U2MODE = 0x8001;
  // enable tx and rx
  U2STA = 0x0400;
}

// Print a string
void uart2_puts(char str[])
{
  int i=0;
  while (str[i] != 0)
  {
    uart2_putc(str[i++]);
  }
}

//Print a character
void uart2_putc(char data)
{
   // make sure buffer is empty
   while(U2STAbits.UTXBF);
   // write character
   U2TXREG = data;
}

// Get a character from UART
unsigned char uart2_getc()
{
  // clear out any overflow error condition
  if (U2STAbits.OERR == 1)
    U2STAbits.OERR = 0;
  // wait until character is ready
  while(!U2STAbits.URXDA);
  return U2RXREG;
}


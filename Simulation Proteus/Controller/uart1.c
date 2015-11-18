/*! \file uart1.c \brief UART function library. */
//*****************************************************************************
// Main Author: Jason Losh
// Modified by Fahad Mirza
//-----------------------------------------------------------------------------
// Objectives and notes             
//-----------------------------------------------------------------------------
// File Name	: 'uart1.c'
// Title		: UART functions
// Created		: 10th March, 2014
// Revised		: 5th April, 2014
// Version		: 1.0

// Target uC:       33FJ128MC802
// Clock Source:    8 MHz primary oscillator set in configuration bits
// Clock Rate:      80 MHz using prediv=2, plldiv=40, postdiv=2
// Devices used:    UART
//*****************************************************************************



//-----------------------------------------------------------------------------
// Device includes and assembler directives             
//-----------------------------------------------------------------------------
#include <p33FJ32MC202.h>
#include <stdio.h>
#include <string.h>
#include "uart1.h"

//-----------------------------------------------------------------------------
// Subroutines                
//-----------------------------------------------------------------------------

// Initializes UART1 with desired Baud rate
void uart1_init(int baud_rate)
{
  // set baud rate
  U1BRG = baud_rate;
  // enable uarts, 8N1, low speed brg
  U1MODE = 0x8000;
  // enable tx and rx
  U1STA = 0x0400;
}

// Print a string
void uart1_puts(const char str[])
{
  int i=0;
  while (str[i] != 0)
  {
    uart1_putc(str[i++]);
  }
}

//Print a character
void uart1_putc(char data)
{
   // make sure buffer is empty
   while(U1STAbits.UTXBF);
   // write character
   U1TXREG = data;
}

// Get a character from UART
char uart1_getc()
{
  // clear out any overflow error condition
  if (U1STAbits.OERR == 1)
    U1STAbits.OERR = 0;
  // wait until character is ready
  while(!U1STAbits.URXDA);
  return U1RXREG;
}


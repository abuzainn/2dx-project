// COMPENG 2DX3
// This program illustrates the interfacing of the Stepper Motor with the microcontroller

//  Written by Ama Simons
//  January 18, 2020
// 	Last Update by Dr. Shahrukh Athar on February 2, 2025

#include <stdint.h>
#include "tm4c1294ncpdt.h"
#include "PLL.h"
#include "SysTick.h"

void PortJ_Init(void) //Port J is for the two onboard user switches (what does this mean?)
{
  //is this right? CHECK LATER
  SYSCTL_RCGCGPIO_R |= 0x08; //enable clock
  while((SYSCTL_PRGPIO_R & 0x08) == 0){};
  GPIO_PORTJ_DIR_R &= 0xFC; // enabling pins 0 and 1 as input
  GPIO_PORTJ_DEN_R |= 0x03; // enabling pins 0 and 1 as GPIO
  GPIO_PORTJ_AFSEL_R &= ~0x03; // disable alt function
  GPIO_PORTJ_AMSEL_R &= ~0x03; // disabling analog functionality
}

void PortM_Init(void) //Port M is for implemented buttons 
{
  SYSCTL_RCGCPIO_R |= SYSCTL_RCGCPIO_R11; //enable clock
  while((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R11) == 0){}; // wait for the clock to stabilize
  GPIO_PORTM_DEN_R |= 0x03; //enabling pins 0 and 1
  GPIO_PORTM_DIR_R &= 0xFC; //making pins as input
  GPIO_PORTM_AFSEL_R &= ~0x03;
  GPIO_PORTM_AMSEL_R &= ~0x03;
}

void PortH_Init(void)
{
  
}

//COMPENG 2DX3
//PROJECT DELIVERABLE 1

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
  GPIO_PORTJ_PDR_R |= 0x03; // enabling internal pulldown
  return;
}

void PortM_Init(void) //Port M is for implemented buttons 
{
  SYSCTL_RCGCPIO_R |= SYSCTL_RCGCPIO_R11; //enable clock
  while((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R11) == 0){}; // wait for the clock to stabilize
  GPIO_PORTM_DEN_R |= 0x03; //enabling pins 0 and 1
  GPIO_PORTM_DIR_R &= 0xFC; //making pins as input
  GPIO_PORTM_PDR_R |= 0x03;
  return;
}

void PortH_Init(void) //Port H is for controlling motor. 
{
  SYSCTL_RCGCPIO_R |= 0x07; //enable clock
  while((SYSCTL_PRGPIO_R & 0x07) == 0){}; 
  GPIO_PORTH_DEN_R |= 0x0F; //enabling pins 0 to 4
  GPIO_PORTH_DIR_R |= 0x0F; //setting pins as output
  return;
}

void PortN_Init(void) //Controls D1 and D2 LEDs
{
  SYSCTL_RCGCPIO_R |= 0x0C; //enable clock
  while((SYSCTL_PRGPIO_R & 0x0C) == 0){};
  GPIO_PORTN_DEN_R |= 0x03; //setting up pins 0 and 1
  GPIO_PORTN_DIR_R |= 0x03; 
  return;
}

void PortF_Init(void) //Controls D3 and D4 LEDs
{
  SYSCTL_RCGCPIO_R |= 0x05;
  while((SYSCTL_PRGPIO_R & 0x05) == 0){};
  GPIO_PORTN_DEN_R |= 0x03;
  GPIO_PORTN_DIR_R |= 0x03;
  return;
}

//BUTTON READING FUNCTIONS, going to use pull up resistor

bool Button0Pressed() //controlled by PJ0
{
  return (GPIO_PORTJ_DATA_R & 0x01) == 1;
}

bool Button1Pressed() //controlled by PJ1
{
  return (GPIO_PORTJ_DATA_R & 0x02) == 1;
}

bool Button2Pressed() //controlled by PM0
{
  return (GPIO_PORTM_DATA_R & 0x01) == 1;
}

bool Button3Pressed() //controlled by PM1
{
  return (GPIO_PORTM_DATA_R & 0x02) == 1;
}

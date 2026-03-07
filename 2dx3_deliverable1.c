//COMPENG 2DX3
//PROJECT DELIVERABLE 1

#include <stdint.h>
#include "tm4c1294ncpdt.h"
#include "PLL.h"
#include "SysTick.h"

//Setting up ints - WE CANNOT USE BOOL
int MotorRunning = 0;
int CW = 1;
int FullStep = 0;

void PortJ_Init(void) //Port J is for the two onboard user switches (what does this mean?)
{
  //is this right? CHECK LATER
  SYSCTL_RCGCGPIO_R |= 0x08; //enable clock
  while((SYSCTL_PRGPIO_R & 0x08) == 0){};
  GPIO_PORTJ_DIR_R &= 0xFC; // enabling pins 0 and 1 as input
  GPIO_PORTJ_PCTL_R &= ~0x000000F0;
  GPIO_PORTJ_DEN_R |= 0x03; // enabling pins 0 and 1 as GPIO
  GPIO_PORTJ_PDR_R |= 0x03; // enabling internal pulldown
  return;
}

void PortM_Init(void) //Port M is for implemented buttons 
{
  SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R11; //enable clock
  while((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R11) == 0){}; // wait for the clock to stabilize
  GPIO_PORTM_DEN_R |= 0x03; //enabling pins 0 and 1
  GPIO_PORTM_DIR_R &= ~0x03; //making pins as input
  GPIO_PORTM_PDR_R |= 0x03;
  return;
}

void PortH_Init(void) //Port H is for controlling motor. 
{
  SYSCTL_RCGCGPIO_R |= 0x07; //enable clock
  while((SYSCTL_PRGPIO_R & 0x07) == 0){}; 
  GPIO_PORTH_DEN_R |= 0x0F; //enabling pins 0 to 4
  GPIO_PORTH_DIR_R |= 0x0F; //setting pins as output
  return;
}

void PortN_Init(void) //Controls D1 and D2 LEDs
{
  SYSCTL_RCGCGPIO_R |= 0x0C; //enable clock
  while((SYSCTL_PRGPIO_R & 0x0C) == 0){};
  GPIO_PORTN_DEN_R |= 0x03; //setting up pins 0 and 1
  GPIO_PORTN_DIR_R |= 0x03; 
  return;
}

void PortF_Init(void) //Controls D3 and D4 LEDs
{
  SYSCTL_RCGCGPIO_R |= 0x05;
  while((SYSCTL_PRGPIO_R & 0x05) == 0){};
  GPIO_PORTF_DEN_R |= 0x11;
  GPIO_PORTF_DIR_R |= 0x11;
  return;
}

//BUTTON READING FUNCTIONS, going to use pull up resistor

int Button0Pressed() //controlled by PJ0
{
  return GPIO_PORTJ_DATA_R & 0x01;
}

int Button1Pressed() //controlled by PJ1
{
  return GPIO_PORTJ_DATA_R & 0x02;
}

int Button2Pressed() //controlled by PM0
{
  return GPIO_PORTM_DATA_R & 0x01;
}

int Button3Pressed()
{
  return GPIO_PORTM_DATA_R & 0x02;
}

void FullStepSpinCW() 
{
	uint32_t delay = 1;	
	int stepCount = 0;
  for(int i = 0; i < 512 && Button0Pressed() == 0; i++)
    {
		GPIO_PORTH_DATA_R = 0b00000011;
		SysTick_Wait10ms(delay);
		GPIO_PORTH_DATA_R = 0b00000110;			
		SysTick_Wait10ms(delay);
		GPIO_PORTH_DATA_R = 0b00001100;	
		SysTick_Wait10ms(delay);
		GPIO_PORTH_DATA_R = 0b00001001;
		SysTick_Wait10ms(delay);
		stepCount++;
		if(stepCount % 64 == 0) //toggling light every 45 deg
		{
			GPIO_PORTF_DATA_R ^= 0x01;
			SysTick_Wait10ms(2);
			GPIO_PORTF_DATA_R ^= 0x01;
		}
    }
	MotorRunning = 0;
	GPIO_PORTN_DATA_R &= ~0x02;
}

void FullStepSpinCCW() 
{
	uint32_t delay = 1;
	int stepCount = 0;
  for(int i = 0; i < 512 && Button0Pressed() == 0; i++)
    {
		GPIO_PORTH_DATA_R = 0b00001001; //0b00001001
		SysTick_Wait10ms(delay);
		GPIO_PORTH_DATA_R = 0b00001100;	//0b00001100	
		SysTick_Wait10ms(delay);
		GPIO_PORTH_DATA_R = 0b00000110;	//0b00000110
		SysTick_Wait10ms(delay);
		GPIO_PORTH_DATA_R = 0b00000011; //0b00000011
		SysTick_Wait10ms(delay);
		stepCount++;
		if(stepCount % 64 == 0)
		{
			GPIO_PORTF_DATA_R ^= 0x01;
			SysTick_Wait10ms(2);
			GPIO_PORTF_DATA_R ^= 0x01;
		}
    
    }
	MotorRunning = 0; //motor should stop after this function is done
	GPIO_PORTN_DATA_R &= ~0x02;
}

void HalfStepSpinCW()
{
	uint32_t delay = 1;
	int stepCount = 0;
	for(int i = 0; i < 512 && Button0Pressed() == 0; i++)
		{
			GPIO_PORTH_DATA_R = 0b00000001;
			SysTick_Wait10ms(delay);
			GPIO_PORTH_DATA_R = 0b00000010;
			SysTick_Wait10ms(delay);
			GPIO_PORTH_DATA_R = 0b00000100;
			SysTick_Wait10ms(delay);
			GPIO_PORTH_DATA_R = 0b00001000;
			SysTick_Wait10ms(delay);
			stepCount++;
			if(stepCount % 16 == 0)
			{
				GPIO_PORTF_DATA_R ^= 0x01;
				SysTick_Wait10ms(2);
				GPIO_PORTF_DATA_R ^= 0x01;
			}
		}
	MotorRunning = 0;
	GPIO_PORTN_DATA_R &= ~0x02;
}

void HalfStepSpinCCW()
{
	uint32_t delay = 1;
	int stepCount = 0;
	for(int i = 0; i < 512 && Button0Pressed() == 0; i++)
		{
			GPIO_PORTH_DATA_R = 0b00001000;
			SysTick_Wait10ms(delay);
			GPIO_PORTH_DATA_R = 0b00000100;
			SysTick_Wait10ms(delay);
			GPIO_PORTH_DATA_R = 0b00000010;
			SysTick_Wait10ms(delay);
			GPIO_PORTH_DATA_R = 0b00000001;
			SysTick_Wait10ms(delay);
			stepCount++;
			if(stepCount % 16 == 0) //blinking light every 11.25 deg
			{
				GPIO_PORTF_DATA_R ^= 0x01;
				SysTick_Wait10ms(2);
				GPIO_PORTF_DATA_R ^= 0x01;
			}	
		}
	MotorRunning = 0;
	GPIO_PORTN_DATA_R &= ~0x02;
}


int main(void)
{
  PLL_Init();
  SysTick_Init();
  PortM_Init();
  PortJ_Init();
  PortH_Init();
  PortN_Init();
  PortF_Init();
  while(1)
  {
    //pressing button 0 toggles motor operation
    if(Button0Pressed())
    {
      if(MotorRunning == 1)//turning off motor
      {
        MotorRunning = 0;
        GPIO_PORTN_DATA_R &= ~0x02; //turning off light
      }
      else //turning on motor
      {
        MotorRunning = 1; 
        GPIO_PORTN_DATA_R |= 0x02; //turning on light
      }
    }
    //Button 1 flips the direction, we will simply toggle between CW and !CW (CCW)
    if(Button1Pressed())
    {
      if(CW == 1) //turning off
      {
        CW = 0;
        GPIO_PORTN_DATA_R &= ~0x01;
      }
      else //turning on
      {
        CW = 1;
        GPIO_PORTN_DATA_R |= 0x01;
      }
    }
    //Button 2 controls the angle, we will toggle the FullStep variable
    if(Button2Pressed())
    {
      if(FullStep == 1)
      {
        FullStep = 0; 
        GPIO_PORTF_DATA_R |= 0x10; //setting PF4 to 1
      }
      else
      {
        FullStep = 1;
        GPIO_PORTF_DATA_R &= ~0x10; //turning off the led
      }
    }
    if(Button3Pressed())
    {
      //this should go home and motor goes to 0 deg. (WHAT DOES THIS MEAN)
      //does this break
    }
  }  
  return 0;
}

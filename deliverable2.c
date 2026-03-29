
#include <stdint.h>
#include "PLL.h"
#include "SysTick.h"
#include "uart.h"
#include "onboardLEDs.h"
#include "tm4c1294ncpdt.h"
#include "VL53L1X_api.h"
#include "stdio.h"
#include <math.h>
#include <stdbool.h>


#define M_PI 3.14159265358979323846

#define I2C_MCS_ACK             0x00000008  // Data Acknowledge Enable
#define I2C_MCS_DATACK          0x00000008  // Acknowledge Data
#define I2C_MCS_ADRACK          0x00000004  // Acknowledge Address
#define I2C_MCS_STOP            0x00000004  // Generate STOP
#define I2C_MCS_START           0x00000002  // Generate START
#define I2C_MCS_ERROR           0x00000002  // Error
#define I2C_MCS_RUN             0x00000001  // I2C Master Enable
#define I2C_MCS_BUSY            0x00000001  // I2C Busy
#define I2C_MCR_MFE             0x00000010  // I2C Master Function Enable

//Motor step configuration
#define STEPS_PER_REV   2048  
#define STEPS_11_25     64
#define STEPS_45        256

#define SEQ_LEN 8

#define MAXRETRIES              5           // number of receive attempts before giving up

#define NUM_MEASUREMENTS  32      // one every 11.25 degrees
#define STEPS_PER_MEAS    64      // 64 half-steps = 11.25 degrees
#define NUM_SCANS         3       // 3 positions along x-axis

int seqIndex = 0; //variable that keeps track of where I am in step sequence
bool MotorRunning = false; //variable that tracks if motor runs (and if I want to scan)


void I2C_Init(void){
  SYSCTL_RCGCI2C_R |= SYSCTL_RCGCI2C_R0;           													// activate I2C0
  SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1;          												// activate port B
  while((SYSCTL_PRGPIO_R&0x0002) == 0){};																		// ready?

    GPIO_PORTB_AFSEL_R |= 0x0C;           																	// 3) enable alt funct on PB2,3       0b00001100
    GPIO_PORTB_ODR_R |= 0x08;             																	// 4) enable open drain on PB3 only

    GPIO_PORTB_DEN_R |= 0x0C;             																	// 5) enable digital I/O on PB2,3
//    GPIO_PORTB_AMSEL_R &= ~0x0C;          																// 7) disable analog functionality on PB2,3

                                                                            // 6) configure PB2,3 as I2C
//  GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xFFFF00FF)+0x00003300;
  GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xFFFF00FF)+0x00002200;    //TED
    I2C0_MCR_R = I2C_MCR_MFE;                      													// 9) master function enable
    I2C0_MTPR_R = 0x08;                       	// 8) configure for 100 kbps clock (added 8 clocks of glitch suppression ~50ns)
//    I2C0_MTPR_R = 0x3B;                                        						// 8) configure for 100 kbps clock
        
}

void PortK_Init(void) {
    SYSCTL_RCGCGPIO_R |= 0x200;
    while ((SYSCTL_PRGPIO_R & 0x000000200) == 0) {}
    GPIO_PORTK_DIR_R  |=  0x0F;
    GPIO_PORTK_DEN_R  |=  0x0F;
    GPIO_PORTK_DATA_R &= ~0x0F;
}

//The VL53L1X needs to be reset using XSHUT.  We will use PG0
void PortG_Init(void){
    //Use PortG0
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R6;                // activate clock for Port N
    while((SYSCTL_PRGPIO_R&SYSCTL_PRGPIO_R6) == 0){};    // allow time for clock to stabilize
    GPIO_PORTG_DIR_R &= 0x00;                                        // make PG0 in (HiZ)
  GPIO_PORTG_AFSEL_R &= ~0x01;                                     // disable alt funct on PG0
  GPIO_PORTG_DEN_R |= 0x01;                                        // enable digital I/O on PG0
                                                                                                    // configure PG0 as GPIO
  //GPIO_PORTN_PCTL_R = (GPIO_PORTN_PCTL_R&0xFFFFFF00)+0x00000000;
  GPIO_PORTG_AMSEL_R &= ~0x01;                                     // disable analog functionality on PN0

    return;
}
static const uint8_t WAVE_DRIVE[8] = {

0x01,
0x02,
0x04,
0x08,
0x01,
0x02,
0x04,
0x08
};

void MotorStep(void){
//go one step in stepper motor. Taken from deliverable 1 code.
	GPIO_PORTK_DATA_R = (GPIO_PORTK_DATA_R & 0xF0) | WAVE_DRIVE[seqIndex];
}

void MotorStepCW(void){
	//one step in CW direction. Used when measuring
	seqIndex = (seqIndex + 1) % SEQ_LEN;
	MotorStep();
}

void MotorStepCCW(void){
	//one step in CCW. FOr use after each scan. Should reduce tangling on wires
	seqIndex = (seqIndex - 1 + SEQ_LEN) % SEQ_LEN;
	MotorStep();
}

void MotorGoHome(void){
	//Return after scan. Reduces tangling on wires
	int total = NUM_MEASUREMENTS * STEPS_PER_MEAS;
	
	for (int i = 0; i < total; i++){
		MotorStepCCW();
		SysTick_Wait10ms(1);
	}
}
//XSHUT     This pin is an active-low shutdown input; 
//					the board pulls it up to VDD to enable the sensor by default. 
//					Driving this pin low puts the sensor into hardware standby. This input is not level-shifted.
void VL53L1X_XSHUT(void){
    GPIO_PORTG_DIR_R |= 0x01;                                        // make PG0 out
    GPIO_PORTG_DATA_R &= 0b11111110;                                 //PG0 = 0
    //FlashAllLEDs();
    SysTick_Wait10ms(10);
    GPIO_PORTG_DIR_R &= ~0x01;                                            // make PG0 input (HiZ)
    
}

uint16_t	dev = 0x29;			//address of the ToF sensor as an I2C slave peripheral
int status=0;


int ToF_ReadDistance(void){
	//A function that reads one distance measurement using the ToF, this should be called every 11.25 degrees
	uint16_t distance;
	uint8_t ready = 0;
	VL53L1X_StartRanging(dev);
	while (ready == 0){ //wait for it to be ready
		VL53L1X_CheckForDataReady(dev, &ready);
		SysTick_Wait10ms(1);
	}
		VL53L1X_GetDistance(dev, &distance);
		VL53L1X_ClearInterrupt(dev);
		VL53L1X_StopRanging(dev);
	  return distance;
}

void Scan(int scanIndex){
    int i;
    uint16_t distance;
    float angle;

    VL53L1X_StartRanging(dev);   //START ONCE

    for(i = 0; i < NUM_MEASUREMENTS; i++){

        LED_MotorOn();
        for (int s = 0; s < STEPS_PER_MEAS; s++){
            MotorStepCW();
            SysTick_Wait10ms(1);
        }
        LED_MotorOff();
				SysTick_Wait10ms(10);

        //WAIT FOR DATA
        uint8_t ready = 0;
        int timeout = 100;

        while ((ready == 0) && (timeout > 0)){
            VL53L1X_CheckForDataReady(dev, &ready);
            SysTick_Wait10ms(2);
            timeout--;
        }

        if(timeout == 0){
            UART_printf("ToF timeout\r\n");
            continue;
        }

        //READ DATA
        VL53L1X_GetDistance(dev, &distance);
        VL53L1X_ClearInterrupt(dev);

        LED_MeasurementFlash();
				
				float angle_deg = i * 11.25;

       // Instead of computing dy and dz, just send raw data (matlab can calculate)
				LED_UARTTxFlash();
			sprintf(printf_buffer, "%d,%.2f,%u\r\n", scanIndex, angle_deg, (unsigned int)distance);
			UART_printf(printf_buffer);
    }

    VL53L1X_StopRanging(dev);   // STOP ONCE

    UART_printf("END\r\n");
}

int main(void) {
	
	int scanIndex = 0;
	int input = 0;
	//initialize
	PLL_Init();	
	SysTick_Init();
	I2C_Init();
	UART_Init();																	
	LED_init();
	PortG_Init();
	PortK_Init();
	
	//ToF getting ready
	
	uint8_t sensorState = 0;
	while(sensorState == 0){
		VL53L1X_BootState(dev, &sensorState);
		SysTick_Wait10ms(10);
	}
	UART_printf("ToF Booted.\r\n");
	VL53L1X_SensorInit(dev); //config, NOTE: 0x29 is the ToF sensor address (it will show up a lot)
	VL53L1X_SetDistanceMode(dev, 2); // 2 = long mode
	VL53L1X_SetTimingBudgetInMs(dev, 100); //currently, 100 is just random
	
	UART_printf("Ready. Press button to scan.\r\n");
	
	while(1){
		//only when the PC tells the MCU to scan does it scan
		while(1){
			input = UART_InChar();
			if (input == 's')
				break;
		}
		LED_UARTTxFlash(); //starting transmission of distance info
		sprintf(printf_buffer, "SCAN, %d\r\n", scanIndex);
		UART_printf(printf_buffer);
			
		Scan(scanIndex);
		MotorGoHome();
		scanIndex++;
		MotorRunning = false;
		}
	}




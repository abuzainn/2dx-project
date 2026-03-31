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

#define I2C_MCS_ACK             0x00000008
#define I2C_MCS_DATACK          0x00000008
#define I2C_MCS_ADRACK          0x00000004
#define I2C_MCS_STOP            0x00000004
#define I2C_MCS_START           0x00000002
#define I2C_MCS_ERROR           0x00000002
#define I2C_MCS_RUN             0x00000001
#define I2C_MCS_BUSY            0x00000001
#define I2C_MCR_MFE             0x00000010

// Motor step configuration
#define STEPS_PER_REV   2048
#define STEPS_11_25     64
#define STEPS_45        256
#define SEQ_LEN         8
#define MAXRETRIES      5

#define NUM_MEASUREMENTS  32      // one every 11.25 degrees
#define STEPS_PER_MEAS    64      // 64 half-steps = 11.25 degrees


volatile bool StartScan = false;
volatile bool StopScan  = false;

int seqIndex    = 0;
bool MotorRunning = false;


void PortJ_Init(void) {
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R8;               // enable clock for Port J
    while ((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R8) == 0) {}   // wait for ready

    GPIO_PORTJ_DIR_R  &= ~0x03;   
    GPIO_PORTJ_DEN_R  |=  0x03;   
    GPIO_PORTJ_PUR_R  |=  0x03;   
	
		//interrupt config
    GPIO_PORTJ_IS_R   &= ~0x03; //edge sensitive   
    GPIO_PORTJ_IBE_R  &= ~0x03;  
    GPIO_PORTJ_IEV_R  &= ~0x03;  
    GPIO_PORTJ_ICR_R  |=  0x03;   
    GPIO_PORTJ_IM_R   |=  0x03;   

    NVIC_PRI12_R = (NVIC_PRI12_R & 0xFF00FFFF) | 0x00A00000; // priority 5 for Port J (IRQ 51)
    NVIC_EN1_R  |= 0x00080000;    // enable IRQ 51 (Port J) in NVIC EN1 bit 19
}


void GPIOJ_IRQHandler(void) { //as a note to self, this only works under this exact name, if I change it the whole thing breaks
    if (GPIO_PORTJ_MIS_R & 0x01) {      // PJ0 starts the scan
        GPIO_PORTJ_ICR_R = 0x01; //interrupt acknowledge
        if (!MotorRunning && !StopScan)  // if the motor is not running, and stopscan is not set (no one pressed PJ1) we set StartScan to 1
            StartScan = true;            //this stops conditions where multiple presses of PJ0 set scans one after another, as well as someone ending scanning, then starting again
    }
    if (GPIO_PORTJ_MIS_R & 0x02) {      // PJ1 stops
        GPIO_PORTJ_ICR_R = 0x02;
        StopScan = true;
    }
}

void I2C_Init(void) { //pulled from studio code.
    SYSCTL_RCGCI2C_R  |= SYSCTL_RCGCI2C_R0;
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1;
    while ((SYSCTL_PRGPIO_R & 0x0002) == 0) {}
    GPIO_PORTB_AFSEL_R |= 0x0C;
    GPIO_PORTB_ODR_R   |= 0x08;
    GPIO_PORTB_DEN_R   |= 0x0C;
    GPIO_PORTB_PCTL_R   = (GPIO_PORTB_PCTL_R & 0xFFFF00FF) + 0x00002200;
    I2C0_MCR_R  = I2C_MCR_MFE;
    I2C0_MTPR_R = 0b0000000000000101000000000111011;
}

void PortK_Init(void) { //Port K manages the motor
    SYSCTL_RCGCGPIO_R |= 0x200;
    while ((SYSCTL_PRGPIO_R & 0x000000200) == 0) {}
    GPIO_PORTK_DIR_R  |=  0x0F;
    GPIO_PORTK_DEN_R  |=  0x0F;
    GPIO_PORTK_DATA_R &= ~0x0F;
}

void PortM_Init(void)
{
	SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R11;
	while ((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R11) == 0) {}
	GPIO_PORTM_DIR_R |=  0x01; //port M0 is a digital output pin. going to use for the clock demo
	GPIO_PORTM_DEN_R |= 0x01;	
}

void PortG_Init(void) { //port G has to do with ToF shutdown. Taken from studios
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R6;
    while ((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R6) == 0) {}
    GPIO_PORTG_DIR_R  &=  0x00;
    GPIO_PORTG_AFSEL_R &= ~0x01;
    GPIO_PORTG_DEN_R  |=  0x01;
    GPIO_PORTG_AMSEL_R &= ~0x01;
}

static const uint8_t WAVE_DRIVE[8] = { 0x01, 0x02, 0x04, 0x08, 0x01, 0x02, 0x04, 0x08 }; //the steps of a wave drive step to the motor

void MotorStep(void) { //move motor one step
    GPIO_PORTK_DATA_R = (GPIO_PORTK_DATA_R & 0xF0) | WAVE_DRIVE[seqIndex];
}

void MotorStepCW(void) { //move motor CW one step. Used when scanning.
    seqIndex = (seqIndex + 1) % SEQ_LEN;
    MotorStep();
}

void MotorStepCCW(void) { //move motor CCW one step. Used when going home
    seqIndex = (seqIndex - 1 + SEQ_LEN) % SEQ_LEN;
    MotorStep();
}

void MotorGoHome(void) {  //rotate 360 degrees CCW after each scan. Helps with detangling the wires. 
    int total = NUM_MEASUREMENTS * STEPS_PER_MEAS;
    LED_MotorOn();
    for (int i = 0; i < total; i++) {
        MotorStepCCW();
        SysTick_Wait10ms(1);
    }
    LED_MotorOff();
}

void VL53L1X_XSHUT(void){ //pulled from studio code
    GPIO_PORTG_DIR_R  |=  0x01;
    GPIO_PORTG_DATA_R &=  0b11111110;
    SysTick_Wait10ms(10);
    GPIO_PORTG_DIR_R  &= ~0x01;
}

uint16_t dev = 0x29; //address of the ToF as a follower in I2C
int status = 0;

void Scan(int scanIndex) {
    int i;
    uint16_t distance;
		LED_UARTTxFlash();
    UART_printf("START\r\n");   // MATLAB uses this to start a new ring

    VL53L1X_StartRanging(dev);

    for (i = 0; i < NUM_MEASUREMENTS; i++) {

        //Check for stop signal between every movement
        if (StopScan) {
            VL53L1X_StopRanging(dev);
						LED_UARTTxFlash();
            UART_printf("END\r\n");   // close the partial ring in MATLAB
            UART_printf("STOP\r\n");  // tell MATLAB to graph now
            while (1) {}              // permanenent stop. PJ1 turns the whole thing off.
        }

        //Stepper motor movements 
        LED_MotorOn();
        for (int s = 0; s < STEPS_PER_MEAS; s++) {
            MotorStepCW();
            SysTick_Wait10ms(1);
        }
        LED_MotorOff();
        SysTick_Wait10ms(10);

        //Wait for ToF data
        uint8_t ready   = 0;
        int     timeout = 100;
        while ((ready == 0) && (timeout > 0)) {
						//if not read, wait, check again until timeout
            VL53L1X_CheckForDataReady(dev, &ready);
            SysTick_Wait10ms(2);
            timeout--;
        }
        if (timeout == 0) {
						LED_UARTTxFlash();
            UART_printf("ToF timeout\r\n");
            continue;
        }

        //read and send distance data
        VL53L1X_GetDistance(dev, &distance);
        VL53L1X_ClearInterrupt(dev);

        LED_MeasurementFlash();
        LED_UARTTxFlash();

        float angle_deg = i * 11.25f;
        sprintf(printf_buffer, "%d,%.2f,%u\r\n", scanIndex, angle_deg, (unsigned int)distance);
        UART_printf(printf_buffer);
    }

    VL53L1X_StopRanging(dev);
		LED_UARTTxFlash();
    UART_printf("END\r\n");
}

int main(void) {
    int scanIndex = 0;
		//init function calls
    PLL_Init();
    SysTick_Init();
    I2C_Init();
    UART_Init();
    LED_init();
    PortG_Init();
    PortK_Init();
    PortJ_Init();
    PortM_Init();

while(1){
	GPIO_PORTM_DATA_R ^= 0x01;
								SysTick_Wait(180000);
								GPIO_PORTM_DATA_R ^= 0x01;
  SysTick_Wait(180000);
  
}
    }
}

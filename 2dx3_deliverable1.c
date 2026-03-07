#include <stdint.h>
#include "tm4c1294ncpdt.h"
#include "SysTick.h"
#include "PLL.h"

// --- Motor step configuration -------------------------------------------------
// 28BYJ-48: 512 half-steps = one full rotation of output shaft
#define STEPS_PER_REV   4096    // half-steps for a full 360 degree rotation
#define STEPS_11_25     128     // half-steps per 11.25 degrees  (512 / 32 = 16)
#define STEPS_45        512     // half-steps per 45 degrees     (512 / 8  = 64)

// --- Half-step sequence for 28BYJ-48 (IN1 IN2 IN3 IN4 on PH0..PH3) ----------
// Adjust bit order if your wiring differs
static const uint8_t HALF_STEP[8] = {
    0x01,  // 0001
    0x03,  // 0011
    0x02,  // 0010
    0x06,  // 0110
    0x04,  // 0100
    0x0C,  // 1100
    0x08,  // 1000
    0x09   // 1001
};
#define SEQ_LEN 8

// --- Globals ------------------------------------------------------------------
volatile uint8_t  motorRunning  = 0;   // 0 = stopped, 1 = running
volatile uint8_t  directionCW   = 1;   // 1 = CW, 0 = CCW
volatile uint8_t  angleFine     = 1;   // 1 = 11.25 deg, 0 = 45 deg
         int      seqIndex      = 0;   // current position in half-step sequence
         int      homeStep      = 0;   // absolute step count at home (0)
         int      currentStep   = 0;   // absolute step count (wraps at STEPS_PER_REV)
         int      stepCount     = 0;   // steps taken since motor started (for auto-stop)
         int      stepsToBlink  = 0;   // steps accumulated toward next blink

// SysTick_Wait10ms(n) is provided by the course header - waits n * 10ms

// --- Port initialisation ------------------------------------------------------
void PortJ_Init(void) {
    SYSCTL_RCGCGPIO_R |= 0x00000100;    // Enable clock for Port J
    while ((SYSCTL_PRGPIO_R & 0x00000100) == 0) {}
    GPIO_PORTJ_DIR_R  &= ~0x03;     // PJ1:PJ0 as inputs
    GPIO_PORTJ_DEN_R  |=  0x03;     // Digital enable
    GPIO_PORTJ_PUR_R  |=  0x03;     // Pull-up resistors (active low: press = 0)
}

void PortM_Init(void) {
    SYSCTL_RCGCGPIO_R |= 0x00000800;    // Enable clock for Port M
    while ((SYSCTL_PRGPIO_R & 0x00000800) == 0) {}
    GPIO_PORTM_DIR_R  &= ~0x03;         // PM1:PM0 as inputs
    GPIO_PORTM_DEN_R  |=  0x03;         // Digital enable
    GPIO_PORTM_PDR_R  &= ~0x03;     // Disable pull-down
    GPIO_PORTM_PUR_R  |=  0x03;     // Pull-up resistors (active low: press = 0)
}

void PortN_Init(void) {
    SYSCTL_RCGCGPIO_R |= 0x00001000;    // Enable clock for Port N
    while ((SYSCTL_PRGPIO_R & 0x00001000) == 0) {}
    GPIO_PORTN_DIR_R  |=  0x03;         // PN1:PN0 as outputs
    GPIO_PORTN_DEN_R  |=  0x03;
    GPIO_PORTN_DATA_R &= ~0x03;         // Start with LEDs off
}

void PortF_Init(void) {
    SYSCTL_RCGCGPIO_R |= 0x00000020;    // Enable clock for Port F
    while ((SYSCTL_PRGPIO_R & 0x00000020) == 0) {}
    GPIO_PORTF_DIR_R  |=  0x11;    // PF4 and PF0 as outputs
    GPIO_PORTF_DEN_R  |=  0x11;
    GPIO_PORTF_DATA_R &= ~0x11;    // Start with LEDs off
}

void PortH_Init(void) {
    SYSCTL_RCGCGPIO_R |= 0x00000080;    // Enable clock for Port H
    while ((SYSCTL_PRGPIO_R & 0x00000080) == 0) {}
    GPIO_PORTH_DIR_R  |=  0x0F;    // PH3:PH0 as outputs
    GPIO_PORTH_DEN_R  |=  0x0F;
    GPIO_PORTH_DATA_R &= ~0x0F;    // Motor coils off
}

// --- LED helpers --------------------------------------------------------------
void LED0_Set(uint8_t on) {             // PN1 - motor running
    if (on) GPIO_PORTN_DATA_R |=  0x02;
    else    GPIO_PORTN_DATA_R &= ~0x02;
}

void LED1_Set(uint8_t on) {             // PN0 - CW direction
    if (on) GPIO_PORTN_DATA_R |=  0x01;
    else    GPIO_PORTN_DATA_R &= ~0x01;
}

void LED2_Set(uint8_t on) {             // PF4 - 11.25 deg mode
    if (on) GPIO_PORTF_DATA_R |=  0x10;
    else    GPIO_PORTF_DATA_R &= ~0x10;
}

void LED3_Toggle(void) {                // PF0 - blink per step
    GPIO_PORTF_DATA_R ^= 0x01;
}

void LED3_Set(uint8_t on) {
    if (on) GPIO_PORTF_DATA_R |=  0x01;
    else    GPIO_PORTF_DATA_R &= ~0x01;
}

void AllLEDs_Clear(void) {
    GPIO_PORTN_DATA_R &= ~0x03;
    GPIO_PORTF_DATA_R &= ~0x11;
}

// --- Button read helpers (active high) ----------------------------------------
uint8_t Button0_Pressed(void) { return (GPIO_PORTJ_DATA_R & 0x01) ? 0 : 1; }
uint8_t Button1_Pressed(void) { return (GPIO_PORTJ_DATA_R & 0x02) ? 0 : 1; }
uint8_t Button2_Pressed(void) { return (GPIO_PORTM_DATA_R & 0x02) ? 0 : 1; }
uint8_t Button3_Pressed(void) { return (GPIO_PORTM_DATA_R & 0x01) ? 0 : 1; }

// --- Motor helpers ------------------------------------------------------------
void Motor_Step(void) {
    GPIO_PORTH_DATA_R = (GPIO_PORTH_DATA_R & 0xF0) | HALF_STEP[seqIndex];
    SysTick_Wait10ms(1);    // ~10 ms per step; adjust n as needed for motor speed
}

void Motor_Off(void) {
    GPIO_PORTH_DATA_R &= ~0x0F;    // De-energise all coils
}

// Advance or retreat one half-step in current direction; update blink counter
void Motor_Advance(void) {
    if (directionCW) {
        seqIndex = (seqIndex + 1) % SEQ_LEN;
        currentStep = (currentStep + 1) % STEPS_PER_REV;
    } else {
        seqIndex = (seqIndex - 1 + SEQ_LEN) % SEQ_LEN;
        currentStep = (currentStep - 1 + STEPS_PER_REV) % STEPS_PER_REV;
    }
    Motor_Step();
    stepCount++;

    // Determine how many steps between each LED3 blink
    int stepsPerBlink = angleFine ? STEPS_11_25 : STEPS_45;
    stepsToBlink++;
    if (stepsToBlink >= stepsPerBlink) {
        stepsToBlink = 0;
        LED3_Toggle();
    }
}

// Return motor to home (0 degrees) via shortest path
void Motor_GoHome(void) {
    // Calculate steps to home going CW vs CCW and pick shorter path
    int stepsForward  = (homeStep - currentStep + STEPS_PER_REV) % STEPS_PER_REV;
    int stepsBackward = (currentStep - homeStep + STEPS_PER_REV) % STEPS_PER_REV;

    int steps;
    if (stepsForward <= stepsBackward) {
        directionCW = 1;
        steps = stepsForward;
    } else {
        directionCW = 0;
        steps = stepsBackward;
    }

    for (int i = 0; i < steps; i++) {
        if (directionCW) {
            seqIndex = (seqIndex + 1) % SEQ_LEN;
            currentStep = (currentStep + 1) % STEPS_PER_REV;
        } else {
            seqIndex = (seqIndex - 1 + SEQ_LEN) % SEQ_LEN;
            currentStep = (currentStep - 1 + STEPS_PER_REV) % STEPS_PER_REV;
        }
        Motor_Step();
    }
}

// --- Debounce helper ----------------------------------------------------------
// Waits for button to be released and adds a short debounce delay
void WaitForRelease_B0(void) { while (Button0_Pressed()) {} SysTick_Wait10ms(2); }
void WaitForRelease_B1(void) { while (Button1_Pressed()) {} SysTick_Wait10ms(2); }
void WaitForRelease_B2(void) { while (Button2_Pressed()) {} SysTick_Wait10ms(2); }
void WaitForRelease_B3(void) { while (Button3_Pressed()) {} SysTick_Wait10ms(2); }

// --- Main ---------------------------------------------------------------------
int main(void) {
    // Initialise peripherals
	  SysTick_Init();
		PLL_Init(); 
    PortJ_Init();
    PortM_Init();
    PortN_Init();
    PortF_Init();
    PortH_Init();

    // Set default state: motor stopped, CW, 11.25 deg mode
    motorRunning = 0;
    directionCW  = 1;
    angleFine    = 1;
    seqIndex     = 0;
    currentStep  = 0;
    homeStep     = 0;
    stepCount    = 0;
    stepsToBlink = 0;

    LED0_Set(0);    // Motor not running
    LED1_Set(1);    // CW direction on by default
    LED2_Set(1);    // 11.25 deg mode on by default
    LED3_Set(0);

    while (1) {

        // -- Button 0: Start / Stop --------------------------------------------
        if (Button0_Pressed()) {
            WaitForRelease_B0();
            motorRunning = !motorRunning;
            if (motorRunning) {
                stepCount    = 0;
                stepsToBlink = 0;
                LED0_Set(1);
                // Restore direction LED
                LED1_Set(directionCW);
                // Restore angle LED only when motor running
                LED2_Set(angleFine);
            } else {
                Motor_Off();
                LED0_Set(0);
                LED2_Set(0);    // LEDs 2 and 3 off when motor stopped
                LED3_Set(0);
            }
        }

        // -- Button 1: Direction toggle ----------------------------------------
        if (Button1_Pressed()) {
            WaitForRelease_B1();
            directionCW = !directionCW;
            LED1_Set(directionCW);
        }

        // -- Button 2: Angle toggle --------------------------------------------
        if (Button3_Pressed()) {
            WaitForRelease_B2();
            angleFine    = !angleFine;
            stepsToBlink = 0;   // Reset blink counter so blinks align to new angle
            if (motorRunning) {
                LED2_Set(angleFine);
            }
        }

        // -- Button 3: Home ----------------------------------------------------
        if (Button2_Pressed()) {
            WaitForRelease_B3();
            Motor_GoHome();
            Motor_Off();
            motorRunning = 0;
            directionCW  = 1;   // Reset to default CW for next run
            stepCount    = 0;
            stepsToBlink = 0;
            AllLEDs_Clear();
            // Restore direction default (CW on) and angle default LED off
            // All LEDs off per spec: "motor ceases operation with all status outputs cleared"
        }

        // -- Motor running: advance one half-step -----------------------------
        if (motorRunning) {
            Motor_Advance();

            // Auto-stop after one full rotation (360 degrees)
            if (stepCount >= STEPS_PER_REV) {
                Motor_Off();
                motorRunning = 0;
                stepCount    = 0;
                stepsToBlink = 0;
                LED0_Set(0);
                LED2_Set(0);
                LED3_Set(0);
            }
        }

    }  // end while(1)
}

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "inc/hw_gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "driverlib/systick.h"
#include "driverlib/adc.h"
#include "driverlib/interrupt.h"
#include "driverlib/pwm.h"
#include "driverlib/pin_map.h"
// Structures


// Globals
volatile uint32_t msTick = 0;
//volatile bool sw1_pressed = false;
//volatile bool sw2_pressed = false;
volatile uint32_t echo_time = 0;
volatile bool new_echo = false;

// Prototypes
void System_Init(void);
void Clock_Init(void);
//void PortF_Init(void);
void PortB_Init(void);
void PortE_Init(void);
void SysTick_Init(void);
void PWM_Init(void);
void Timer_Init(void);
//void ADC_Init(void);
void SysTick_ISR(void);
//void SwitchDebounce(void);
//void Switch_Action(void);
void Echo_Capture(void);
void Echo_Return(void);


// Initializations

void System_Init(void){
    Clock_Init();
//    PortF_Init();
    PortB_Init();
    PortE_Init();
    SysTick_Init();
    PWM_Init();
//    ADC_Init();
    Timer_Init();

}

void Clock_Init(void) {
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ); // 80 MHz System Clock
    SysCtlPWMClockSet(SYSCTL_PWMDIV_32);
}

//void PortF_Init(void){
//    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF); // Enable Port F
//        while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF)) {}
//    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY; // Unlock Switch 2
//    HWREG(GPIO_PORTF_BASE + GPIO_O_CR) |= GPIO_PIN_0;
//    GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_4 | GPIO_PIN_0); // Set   switches as inputs
//    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4 | GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU); // Configure switches as pull up (Active Low)
//    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2); // Set LEDs as Outputs
//    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2, 0x00); // Clear LEDs to start
//}

void PortB_Init(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB); // Enable Port B
        while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB)) {}
        GPIOPinConfigure(GPIO_PB4_T1CCP0);
        GPIOPinTypeTimer(GPIO_PORTB_BASE, GPIO_PIN_4); // Configure Pin B4 For Timer 1A *Pulse*
        GPIOPinConfigure(GPIO_PB5_T1CCP1);
        GPIOPinTypeTimer(GPIO_PORTB_BASE, GPIO_PIN_5); // Configure Pin B5 For Timer 1B *Echo*
}

void PortE_Init(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE); // Enable Port E
        while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE)) {}
        GPIOPinTypePWM(GPIO_PORTE_BASE, GPIO_PIN_4); // Configure Pin E4 for PWM 0, Gen 2 *servo*
        GPIOPinConfigure(GPIO_PE4_M0PWM4); // Connect the GPIO Pin to the PWM Output
}

void SysTick_Init(void) {
    SysTickDisable();
    SysTickPeriodSet(80000); // On 80 MHz clock, 80,000,000 cycles * .001 seconds
    SysTickIntRegister(SysTick_ISR);
    SysTickIntEnable();
    SysTickEnable();
}

void PWM_Init(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0); // Enable PWM 0
        while (!SysCtlPeripheralReady(SYSCTL_PERIPH_PWM0)){}
    PWMGenConfigure(PWM0_BASE, PWM_GEN_2, PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC); // Configure Gen 2 (Chosen based on outputs)
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_2, 25000); // Set the period (Clock / Divisor = PWM Clock, PWM Clock / Desired Hz) *Servo*
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_4, 1875); // Set initial output to center
    PWMOutputState(PWM0_BASE, PWM_OUT_4_BIT, true); // Enable PWM Output
}

void Timer_Init(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1); // Enable Timer 1
        while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER1)){}
    TimerConfigure(TIMER1_BASE, (TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_ONE_SHOT_PWM |
    TIMER_CFG_B_CAP_TIME_UP)); // Configure Timer 1 Types (In this case split with one count down pwm for a pulse, and one capture timer for echo)
    TimerLoadSet(TIMER1_BASE, TIMER_A, 880); // PWM timer count down (800 for 10 us plus buffer)
    TimerMatchSet(TIMER1_BASE, TIMER_A, 80); // Push pin low at setting
    TimerControlEvent(TIMER1_BASE, TIMER_B, TIMER_EVENT_BOTH_EDGES); // Will trigger capture on rising or falling edge
    TimerPrescaleSet(TIMER1_BASE, TIMER_B, 0xFF); // Scale the timer for longer capture time.
    TimerIntRegister(TIMER1_BASE, TIMER_B, Echo_Capture); // Register ISR handler
    TimerIntEnable(TIMER1_BASE, TIMER_CAPB_EVENT); // Triggers interrupt on a capture event
    TimerEnable(TIMER1_BASE, TIMER_B); // Enable timer
}

//void ADC_Init(void){
//    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0); // Enable ADC 0
//        while (!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0)){}
//    ADCHardwareOversampleConfigure(ADC0_BASE, 64); // Set ADC to over sample to smooth out noise
//    ADCSequenceStepConfigure(ADC0_BASE, 3, 0,
//    ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END); // Configure Sequencer 3 Channel and Steps
//    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0); // Set ADC Sequencer 3 to trigger on manual ISR with high priority
//    ADCSequenceEnable(ADC0_BASE, 3); // Enable Sequencer
//    ADCIntClear(ADC0_BASE, 3); // Clear ISR
//    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3); // Set pin E3 to ADC Input
//}

// ISRs

void SysTick_ISR(void) {
    //Background system tick at 1 ms
    msTick++;
}

void Echo_Capture(void){
    static uint32_t RisingTime = 0;
    uint32_t Status = TimerIntStatus(TIMER1_BASE, true); // Check valid interrupt
    if(Status & TIMER_CAPB_EVENT){
        TimerIntClear(TIMER1_BASE, TIMER_CAPB_EVENT); // Clear
        uint32_t CurrentTime = TimerValueGet(TIMER1_BASE, TIMER_B); // Take a snapshot of the timer value
        if(GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_5) & GPIO_PIN_5){
            RisingTime = CurrentTime; // If a rising edge save snapshot as start time
        }else{
            echo_time = (CurrentTime - RisingTime) & 0x00FFFFFF; // Subtract the values (Masked for variable size differences)
            new_echo = true; // Flag new echo
        }
    }
}

//Functions

//void SwitchDebounce(void) {
//    // Checks switch reads for consecutive states to indicate a positive press post debouncing
//    static uint16_t SwitchStates[2] = {0, 0}; // Init switch state array
//    uint16_t sw1_bit = (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4) >> 4) & 0x01; // Read and shift to bit 0
//    SwitchStates[0] = (SwitchStates[0] << 1) | sw1_bit | 0xe000; // Store shifted value, mask, and read bit into index 0
//    if (SwitchStates[0] == 0xf000) { // If 12 consecutive 0s are read confirm switch press
//        sw1_pressed = true;
//    }
//    uint16_t sw2_bit = GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_0) & 0x01; // Read input, no shift needed
//    SwitchStates[1] = (SwitchStates[1] << 1) | sw2_bit | 0xe000; // Store shifted value, mask, and read bit into index 1
//    if (SwitchStates[1] == 0xf000) { // If 12 consecutive 0s are read confirm switch press
//        sw2_pressed = true;
//    }
//}

//void Switch_Action(void) {
//    // Will execute actions based on switch press
//    if (sw1_pressed) {
//        sw1_pressed = false; // Clear the switch to execute only once
//        Sw2_Action(); // Process action due to the switch as separate function or in this one
//    }
//    if (sw2_pressed) {
//        sw2_pressed = false; // Clear the switch to execute only once
//        Sw2_Action(); // Process action due to the switch as separate function or in this one
//    }
//
//}

void Echo_Return(void){
    float Distance_cm = (float)echo_time * 0.0002144f; // Convert cycles to distance (\frac{1}{80M} * 343 * 100)
    if(Distance_cm <= 0.0f ){Distance_cm = 0.0f;} // Bound for safety
    if(Distance_cm >= 100.0f){Distance_cm = 100.0f;} //Bound for safety
    uint32_t servo_duty = 625 + (uint32_t)(Distance_cm * 25.0f); // Convert back to cycles with offset
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_4, servo_duty); // Set cycles for angle
    new_echo = false; // Clear flag
}

int main(void) {
    System_Init();
//    uint32_t lastDebounceTime = 0;
    uint32_t lastPulseTime = 0;
    while(1) {
//        if ((msTick - lastDebounceTime) >= 2) {
//            lastDebounceTime = msTick;
//            SwitchDebounce();
//        }
//        if (sw1_pressed || sw2_pressed){
//            Switch_Action();
//        }
        if ((msTick - lastPulseTime) >= 60 ){
            TimerEnable(TIMER1_BASE, TIMER_A);
        }
        if(new_echo){
            Echo_Returen();
        }
    }
    return 0;
}

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
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

//Definitions
#define MOTOR_PINS (GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7)
#define ADC_PIN GPIO_PIN_0
#define MOTOR_PORT GPIO_PORTC_BASE
#define ADC_PORT GPIO_PORTE_BASE
#define ADC_DARK_OFFSET 31
#define ADC_MIN_SIGNAL  20

// Structures
typedef struct{
    volatile int32_t position;
    volatile uint32_t remaining;
    volatile uint8_t phase;
    volatile int8_t direction;
    volatile bool moving;
}MotorState;

typedef struct{
    volatile bool new_value;
    volatile uint32_t raw_read[1];
}ADCData;

// Variables
volatile uint32_t msTick = 0;
static const uint8_t sequence[8] = {0x10, 0x30, 0x20, 0x60, 0x40, 0xC0, 0x80, 0x90};
volatile MotorState motor = {0,0,0,0,false};
volatile ADCData ADC = {false, 0};
const float STEPS_PER_LOG = 42975.49f;


// Prototypes
void update_motor_target(void);
void system_init(void);
void ADC_boot(void);
void ADC_init(void);
void ADC_read(void);
void clock_init(void);
void portC_init(void);
void portE_init(void);
void timer_init(void);
void motor_movement(void);
uint32_t adc_for_log(uint32_t raw);

// Initializations
void system_init(void){
    clock_init();
    portC_init();
    portE_init();
    timer_init();
    ADC_init();
    ADC_boot();
}



void clock_init(void) {
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ); // 80 MHz System Clock
    SysCtlPWMClockSet(SYSCTL_PWMDIV_32);
}

void portC_init(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC); // Enable Port C
        while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOC)) {}
    GPIOPinTypeGPIOOutput(MOTOR_PORT, MOTOR_PINS); // Pins C 4 to 7

}

void portE_init(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE); // Enable Port C
        while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE)) {}
    GPIOPinTypeADC(ADC_PORT, ADC_PIN); //Pin E0

}

void timer_init(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_WTIMER1); // Enable Timer 1
        while(!SysCtlPeripheralReady(SYSCTL_PERIPH_WTIMER1)){}
    TimerConfigure(WTIMER1_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_PERIODIC | TIMER_CFG_B_PERIODIC); // Configure wide timer to 32 bit halves
    TimerLoadSet(WTIMER1_BASE, TIMER_A, 80000000); // 1 sec load for timer
    TimerLoadSet(WTIMER1_BASE, TIMER_B, 160000); // 5 ms load
    TimerIntRegister(WTIMER1_BASE, TIMER_B, motor_movement);
    TimerIntEnable(WTIMER1_BASE, TIMER_TIMB_TIMEOUT);
    TimerEnable(WTIMER1_BASE, TIMER_A); // Enable timer
}

void ADC_init(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0); // Enable ADC 0
        while (!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0)){}
    ADCHardwareOversampleConfigure(ADC0_BASE, 64); // Set ADC to over sample to smooth out noise
    ADCSequenceStepConfigure(ADC0_BASE, 3, 0,
    ADC_CTL_CH3 | ADC_CTL_IE | ADC_CTL_END); // Configure Sequencer 3 Channel and Steps
    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0); // Set ADC Sequencer 3 to trigger on timer ISR with high priority
    ADCSequenceEnable(ADC0_BASE, 3); // Enable Sequencer
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0); // Set pin E0 to ADC Input
}

void ADC_boot(void){
   ADCProcessorTrigger(ADC0_BASE, 3);
       while(!ADCIntStatus(ADC0_BASE, 3, false));
   ADCIntClear(ADC0_BASE, 3);
   uint32_t ADC_boot_value =0;
   ADCSequenceDataGet(ADC0_BASE, 3, &ADC_boot_value);
   motor.position = (int32_t)(logf((float)adc_for_log(ADC_boot_value)) * STEPS_PER_LOG);
   ADCSequenceDisable(ADC0_BASE, 3); // Enable Sequencer
   ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_TIMER, 0);
   TimerControlTrigger(WTIMER1_BASE, TIMER_A, true); // Set ADC to trigger on timer interrupt
   ADCSequenceEnable(ADC0_BASE, 3); // Enable Sequencer
   ADCIntRegister(ADC0_BASE, 3, ADC_read); // ISR register for ADC
   ADCIntClear(ADC0_BASE, 3); // Clear ISR
   ADCIntEnable(ADC0_BASE, 3); // Enable ISR
}

//ISRs

void motor_movement(void){
    TimerIntClear(WTIMER1_BASE, TIMER_TIMB_TIMEOUT);
    if(!motor.moving){
        return;
    }
    if(motor.remaining == 0){
        motor.moving = false;
        GPIOPinWrite(MOTOR_PORT, MOTOR_PINS, 0);
        TimerDisable(WTIMER1_BASE, TIMER_B);
        return;
    }
    motor.phase = (motor.phase + motor.direction + 8) & 7;
    GPIOPinWrite(MOTOR_PORT, MOTOR_PINS, sequence[motor.phase]);
    motor.position += motor.direction;
    motor.remaining --;

}

void ADC_read(void){
    ADCIntClear(ADC0_BASE, 3); // Clear Flag
    ADCSequenceDataGet(ADC0_BASE, 3, (uint32_t *)ADC.raw_read);
    ADC.new_value = true;
}

// Functions

uint32_t adc_for_log(uint32_t raw){
    uint32_t v = (raw > ADC_DARK_OFFSET) ? (raw - ADC_DARK_OFFSET) : 0;
    return (v < ADC_MIN_SIGNAL) ? ADC_MIN_SIGNAL : v;
}

void update_motor_target(void){
    ADC.new_value = false;
    uint32_t local_adc_value = ADC.raw_read[0];
    static float filtered = -1.0f;
    float x = (float)adc_for_log(ADC.raw_read[0]);
    filtered = (filtered < 0) ? x : filtered + 0.25f * (x - filtered);
    float brightness = logf(filtered);
    int32_t motor_target = (int32_t)(brightness * STEPS_PER_LOG);
    int32_t deadband = (int32_t)(logf(1.0f + 2.0f / 100.0f) * STEPS_PER_LOG);
    _disable_interrupts();
    int32_t motor_steps_needed = motor_target - motor.position;
    if (motor.moving || (abs(motor_steps_needed) > deadband)){
        motor.remaining = abs(motor_steps_needed);
        if(motor_steps_needed != 0){
            motor.direction = (motor_steps_needed > 0) ? 1 : -1;
        if (!motor.moving)
        {
            motor.moving = true;
            TimerEnable(WTIMER1_BASE, TIMER_B);
        }
    }
    }
    _enable_interrupts();
}

int main(void) {
    system_init();
    while(1) {
        if (ADC.new_value){
            update_motor_target();
        }
    }
    return 0;
}

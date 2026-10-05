#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "driverlib/systick.h"
#include "driverlib/adc.h"
#include "driverlib/interrupt.h"
#include "driverlib/pwm.h"
#include "driverlib/pin_map.h"

// Prototype all functions here
void GPIO_Init(void);
void SysTick_Init(void);
void GPIO_ISR(void);
void SysTick_ISR(void);
void SwCheck(void);
void LED_State(void);
void PWM_Init(void);
void ADC_Init(void);
int read_adc(void);
void Check_Brightness(void);

//Globals
volatile uint32_t msTicks = 0;

//Structs
typedef struct
{
    volatile bool lock;
    volatile bool turned;
    uint8_t brightness;
    uint16_t last;
} knob_t;

knob_t knob = { false, false, 0, 0 };

typedef struct
{
    bool pressed;
    volatile bool debounce;
    volatile uint32_t ms_start;
} Button_t;

Button_t sw1 = { false, false, 0 };

//Functions
void GPIO_Init(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF))
    {
    }
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2, GPIO_STRENGTH_2MA,
    GPIO_PIN_TYPE_STD);
    GPIOIntRegister(GPIO_PORTF_BASE, GPIO_ISR);
    GPIOIntTypeSet(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_FALLING_EDGE);
    GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_4);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA,
    GPIO_PIN_TYPE_STD_WPU);
    GPIOIntClear(GPIO_PORTF_BASE, GPIO_PIN_4);
    GPIOIntEnable(GPIO_PORTF_BASE, GPIO_PIN_4);
}

void PWM_Init(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM1);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_PWM1))
    {
    }
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_3);
    GPIOPinConfigure(GPIO_PF3_M1PWM7);
    PWMGenConfigure(PWM1_BASE, PWM_GEN_3,
    PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC);
    PWMGenPeriodSet(PWM1_BASE, PWM_GEN_3, 3200);
    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_7, 1);
    PWMGenEnable(PWM1_BASE, PWM_GEN_3);
    PWMOutputState(PWM1_BASE, PWM_OUT_7_BIT, true);
}

void ADC_Init(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0))
    {
    }
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_5);
    ADCHardwareOversampleConfigure(ADC0_BASE, 64);
    HWREG(ADC0_BASE + 0x38) |= 0x40;
    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC0_BASE, 3, 0,
    ADC_CTL_CH8 | ADC_CTL_IE | ADC_CTL_END);
    ADCSequenceEnable(ADC0_BASE, 3);
    ADCIntClear(ADC0_BASE, 3);
}

int read_adc(void)
{
    uint32_t value = 0;
    ADCProcessorTrigger(ADC0_BASE, 3);
    while (!ADCIntStatus(ADC0_BASE, 3, false))
        ;
    ADCIntClear(ADC0_BASE, 3);
    ADCSequenceDataGet(ADC0_BASE, 3, &value);
    return (int) value;
}

void SysTick_Init(void)
{
    SysTickPeriodSet(80000);
    SysTickIntRegister(SysTick_ISR);
    SysTickIntEnable();
    SysTickEnable();
}

void GPIO_ISR(void)
{
    uint32_t status = GPIOIntStatus(GPIO_PORTF_BASE, true);
    GPIOIntClear(GPIO_PORTF_BASE, GPIO_PIN_4);
    if (status & GPIO_PIN_4)
    {
        GPIOIntDisable(GPIO_PORTF_BASE, GPIO_PIN_4);
        sw1.debounce = true;
        sw1.ms_start = msTicks;
    }
}

void SysTick_ISR(void)
{
    msTicks++;
}

void SwCheck(void){
    if (sw1.debounce)
        {
            if ((msTicks - sw1.ms_start) >= 15)
            {
                if ((GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4) & GPIO_PIN_4) == 0)
                {
                    sw1.pressed = true;
                }
                sw1.debounce = false;
                GPIOIntEnable(GPIO_PORTF_BASE, GPIO_PIN_4);
            }
        }
}

void Check_Brightness(void)
{
    uint16_t current = read_adc();
    if (current > 4095){
        current = 4095;
    }
    if (abs(current - knob.last) > 20)
    {
        knob.last = current;
        knob.brightness = (uint8_t)((current * 30) / 4095);
        knob.turned = true;
    }
}

void LED_State(void){
    if(sw1.pressed && ((GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4) & GPIO_PIN_4) == 0)){
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2, 0);
        uint32_t pulseWidth = (knob.brightness * 3200) / 30;
        if (pulseWidth == 0){
            pulseWidth = 1;
        }
        if (pulseWidth >= 3199){
            pulseWidth = 3199;
        }
        PWMPulseWidthSet(PWM1_BASE, PWM_OUT_7, pulseWidth);
    }else{
        sw1.pressed = 0;
        PWMPulseWidthSet(PWM1_BASE, PWM_OUT_7, 1);
        static uint32_t cycle = 0;
        uint32_t current = msTicks;
        if (current - cycle >= 5000){
            cycle += 5000;
        }
        uint32_t count = current - cycle;
        if(count < 1000){
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2, GPIO_PIN_2);
        }else if(count < 3000){
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2, GPIO_PIN_1);
        }else{
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2, 0);
        }
    }
}

int main(void)
{
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);
    GPIO_Init();
    SysTick_Init();
    PWM_Init();
    ADC_Init();

    while (1)
    {
        Check_Brightness();
        SwCheck();
        LED_State();
    }
    return 0;
}




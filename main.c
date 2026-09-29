#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "driverlib/systick.h"
//#include "driverlib/adc.h"
#include "driverlib/interrupt.h"
//#include "driverlib/pwm.h"
#include "driverlib/pin_map.h"

// Prototype all functions here
void GPIO_Init(void);
void SysTick_Init(void);
void GPIO_ISR(void);
void SysTick_ISR(void);
void SwCheck(void);
void LED_State(void);

//Globals
volatile uint32_t msTicks = 0;

//Structs
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
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_STRENGTH_2MA,
    GPIO_PIN_TYPE_STD);
    GPIOIntRegister(GPIO_PORTF_BASE, GPIO_ISR);
    GPIOIntTypeSet(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_FALLING_EDGE);
    GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_4);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA,
    GPIO_PIN_TYPE_STD_WPU);
    GPIOIntClear(GPIO_PORTF_BASE, GPIO_PIN_4);
    GPIOIntEnable(GPIO_PORTF_BASE, GPIO_PIN_4);
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0x0);
}

void SysTick_Init(void)
{
    SysTickPeriodSet(16000);
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

void LED_State(void){
    if(sw1.pressed && ((GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4) & GPIO_PIN_4) == 0)){
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_3);
    }else{
        sw1.pressed = 0;
        static uint16_t cycle = 0;
        uint32_t current = msTicks;
        if (current - cycle >= 5000){
            cycle += 5000;
        }
        uint32_t count = current - cycle;
        if(count < 1000){
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_2);
        }else if(count < 3000){
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_1);
        }else{
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 0);
        }
    }
}

int main(void)
{
    GPIO_Init();
    SysTick_Init();
    while (1)
    {
        SwCheck();
        LED_State();
    }
    return 0;
}




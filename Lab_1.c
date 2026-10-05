/* EENG 461 Lab 1 program listing */
#include <stdio.h>

#define GPIO_PORTF_AFSEL_R (*((volatile unsigned long *)0x40025420))
#define GPIO_PORTF_AMSEL_R (*((volatile unsigned long *)0x40025528))
#define GPIO_PORTF_DATA_R (*((volatile unsigned long *)0x400253FC))
#define GPIO_PORTF_DATA_R_PF1 (*((volatile unsigned long *)0x40025008))
#define GPIO_PORTF_DEN_R (*((volatile unsigned long *)0x4002551C))
#define GPIO_PORTF_DIR_R (*((volatile unsigned long *)0x40025400))
#define GPIO_PORTF_PCTL_R (*((volatile unsigned long *)0x4002552C))
#define SYSCTL_RCGCGPIO_R (*((volatile unsigned long *)0x400FE608))
#define SYSCTL_PRGPIO_R (*((volatile unsigned long *)0x400FEA08))
#define NVIC_ST_CTRL_R (*((volatile unsigned long *)0xE000E010))
#define NVIC_ST_RELOAD_R (*((volatile unsigned long *)0xE000E014))
#define NVIC_ST_CURRENT_R (*((volatile unsigned long *)0xE000E018))
void Config_SysTick(void)
{
NVIC_ST_CTRL_R = 0x0;
NVIC_ST_RELOAD_R = 0x00FFFFFF;
NVIC_ST_CURRENT_R = 0x0;
NVIC_ST_CTRL_R = 0x00000005;
}
void Config_Port_F(void)
{
SYSCTL_RCGCGPIO_R |= 0x20; // Enable clock for Port F
while((SYSCTL_PRGPIO_R&0x20) == 0){}; // Check if Port F is ready
GPIO_PORTF_AMSEL_R &= ~0x0E; // Disable analog function for PF1-PF3
GPIO_PORTF_PCTL_R &= ~0x0000FFF0; // Configure PF1-PF3 as I/O ports
GPIO_PORTF_DIR_R |= 0x0E; // PF1-PF3 set as outputs
GPIO_PORTF_AFSEL_R &= ~0x0E; // Disable alternate function for PF1-PF3
GPIO_PORTF_DEN_R |= 0x0E; // Enable digital I/O for PF1-PF3
GPIO_PORTF_DATA_R &= ~0x0E; // Clear PF1-PF3
}
void SysTick_delay(void)
{
NVIC_ST_RELOAD_R = 0x3E7F; // Load value for 1 ms delay
NVIC_ST_CURRENT_R = 0x0; // Clear current and start timer
while((NVIC_ST_CTRL_R & 0x00010000) == 0){}; // Wait until COUNT flag is set
}
void ms_delay(unsigned long count)
{
unsigned long i;
for(i=0; i<count; i++)
{
SysTick_delay();
}
}
int main(void)
{
Config_SysTick();
Config_Port_F();
while(1)
{
printf("The LED Beeped! \n");
ms_delay(498); // 996 ms per blink, 60 blinks per second
GPIO_PORTF_DATA_R ^= 0x0C; /* Toggle PF2 and PF3 together */
//GPIO_PORTF_DATA_R ^= 0x02; /* Toggle PF1 alone */
//GPIO_PORTF_DATA_R_PF1 ^= 0x02; /* Toggle PF1 alone (alternate address) */
}
return 0;
}

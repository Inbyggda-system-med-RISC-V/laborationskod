#include "gd32vf103.h"
#include "drivers.h"
#include "dac.h"
#include "pwm.h"
#include "adc.h"
#include "eclicw.h"

void mt40k_init(void (*pISR)(void)){
   eclicw_enable(CLIC_INT_TMR, 1, 1, pISR);
   // Set the timer expier (compar) value to 675 (27Mhz/40kHz).
   *( volatile uint64_t * )( TIMER_CTRL_ADDR + TIMER_MTIMECMP ) = 675;
   // Reset the timer value to zero.
   *( volatile uint64_t * )( TIMER_CTRL_ADDR + TIMER_MTIME ) = 0;
}
/*
void lp(void){
    static int x=0;

    // Be aware of possible spirous int updating mtimecmp...
    // LSW = -1; MSW = update; LSW = update, in this case safe.
    *( volatile uint64_t * )( TIMER_CTRL_ADDR + TIMER_MTIMECMP )+=675;

    // Calcylate next output value...
    DAC0set(x++%2?4000:0);
    l88mem(7,x);
}
*/

void lp(void){
    static int x=0, xp=0, y=0, yp=0;
    static int g=0b0010001100000000; //0.1367287359(<<16)
    static int p=0b1011100111111110; //0.7265425280(<<16)

    // Be aware of possible spirous int updating mtimecmp...
    // LSW = -1; MSW = update; LSW = update, in this case safe.
    *( volatile uint64_t * )( TIMER_CTRL_ADDR + TIMER_MTIMECMP )+=675;

    // Calcylate next output value...
    xp=x; yp=y;
    x = ((adc_regular_data_read(ADC0)-2048))*g;
    y = (x+xp)+(int)(((int64_t)yp*(int64_t)p)>>16);
    DAC0set((y>>16)+2048);              // ...and present it to hw-driver.

    adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
}

int main(void){
    int ms=0, s=0, key, pKey=-1, c=0, idle=0;
    int lookUpTbl[16]={1,4,7,14,2,5,8,0,3,6,9,15,10,11,12,13};
    int dac=0, speed=-100;
    int adcr, tmpr;

    t5omsi();                               // Initialize timer5 1kHz
    colinit();                              // Initialize column toolbox
    l88init();                              // Initialize 8*8 led toolbox
    keyinit();                              // Initialize keyboard toolbox
    //DAC0powerUpInit();                      // Initialize DAC0/PA4 toolbox
    //T1powerUpInitPWM(0x4);                  // Timer #1, Ch #2 PWM
    //T1powerUpInitPWM(0xC);                  // Timer #1, Ch #2 & 3 PWM
    ADC3powerUpInit(1);                     // Initialize ADC0, Ch3 & Ch16
    //mt40k_init(&lp);                        // Initialize mtime 40kHz

    //eclic_global_interrupt_enable();        // !!!!! Enable Interrupt !!!!!

    while (1) {
        idle++;                             // Manage Async events

        if (adc_flag_get(ADC0,ADC_FLAG_EOC)==SET) { // ...ADC done?
          if (adc_flag_get(ADC0,ADC_FLAG_EOIC)==SET) { //...ch3 or ch16?
            tmpr = adc_inserted_data_read(ADC0, ADC_INSERTED_CHANNEL_0);
            l88mem(6,((0x680-tmpr)/5)+25);
            adc_flag_clear(ADC0, ADC_FLAG_EOC);
            adc_flag_clear(ADC0, ADC_FLAG_EOIC);
          } else {
            adcr = adc_regular_data_read(ADC0); // ......get data
            l88mem(4,adcr>>8);                  // ......move data
            l88mem(5,adcr);                     // ......(view each ms)
            adc_flag_clear(ADC0, ADC_FLAG_EOC); // ......clear IF
          }
        }

        if (t5expq()) {                     // Manage periodic tasks
            l88row(colset());               // ...8*8LED and Keyboard
            ms++;                           // ...One second heart beat
            if (ms==1000){
              ms=0;
              l88mem(0,s++);
              //T1setPWMmotorB(speed+=10);    // PWM T1/C23 Unit test
              //if (speed>=100) speed=-100;
            }
            if ((key=keyscan())>=0) {       // ...Any key pressed?
              if (pKey==key) c++; else {c=0; pKey=key;}
              l88mem(1,lookUpTbl[key]+(c<<4));
            }
            l88mem(2,idle>>8);              // ...Performance monitor
            l88mem(3,idle); idle=0;
            //DAC0set(dac++);                   // DAC0 Unit test
            //T1setPWMch2(15000);               // PWM T1/C2 Unit test
            adc_software_trigger_enable(ADC0, //Trigger another ADC conversion!
                                        ADC_REGULAR_CHANNEL);
        }
    }
}
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#define UART_ID uart0
#define BAUD_RATE 100000
#define DATA_BITS 8
#define STOP_BITS 2
#define PARITY    UART_PARITY_EVEN
#define DUTYMIN 1330
#define DUTYMAX 2315
#define CH3MAX 1707
#define CH3MIN 321


//０番と1番ピンに接続
#define UART_TX_PIN 0
#define UART_RX_PIN 1

//グルーバル変数の宣言
static int chars_rxed = 0;
static int data_num=0;
uint8_t sbus_data[25];
uint8_t ch;
uint slice_num;
uint16_t Chdata[6];

//関数の宣言
uint8_t init_serial(void);
uint8_t init_pwm();
void on_uart_rx(); 

uint8_t init_serial(void){

    /// シリアル通信の設定

    // UARTを基本の通信速度で設定
    uart_init(UART_ID, 2400);

    // 指定のGPIOをUARTのTX、RXピンに設定する
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    //指定のUARTを指定の通信速度に設定する
    int actual = uart_set_baudrate(UART_ID, BAUD_RATE);

    //UART flow control CTS/RTSを使用しない設定
    uart_set_hw_flow(UART_ID, false, false);

    //通信フォーマットの設定
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

    // Turn off FIFO's - we want to do this character by character
    uart_set_fifo_enabled(UART_ID, false);

    // Set up a RX interrupt
    // We need to set up the handler first
    // Select correct interrupt for the UART we are using
    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

    // And set up and enable the interrupt handlers
    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(UART_ID, true, false);
}

uint8_t init_pwm(){

   // PWMの設定
   // Tell GPIO 0 and 1 they are allocated to the PWM
    gpio_set_function(2, GPIO_FUNC_PWM);
    gpio_set_function(3, GPIO_FUNC_PWM);

    // Find out which PWM slice is connected to GPIO 0 (it's slice 0)
    slice_num = pwm_gpio_to_slice_num(3);

    // Set period T
    // T=(wrap+1)*clkdiv/sysclock
    // T=(24999+1)*100/125e6=25000e2/125e6=200e-4=0.02s(=50Hz)
    pwm_set_wrap(slice_num, 24999);
    pwm_set_clkdiv(slice_num, 100.0);
    // Set channel A Duty
    // DutyA=clkdiv*PWM_CHAN_A/sysclock
    pwm_set_chan_level(slice_num, PWM_CHAN_A, DUTYMAX);
    // Set initial B Duty
    // DutyB=clkdiv*PWM_CHAN_B/sysclock
    pwm_set_chan_level(slice_num, PWM_CHAN_B, DUTYMIN);
    // Set the PWM running
    pwm_set_enabled(slice_num, true);
    sleep_ms(2000);
    pwm_set_chan_level(slice_num, PWM_CHAN_A, DUTYMIN);
}

// RX interrupt handler
void on_uart_rx() {
    short data;
    while (uart_is_readable(UART_ID)) {
        ch = uart_getc(UART_ID);
        if(ch==0x0f&&chars_rxed==00){
            sbus_data[chars_rxed]=ch;
            //printf("%02X ",ch);
            chars_rxed++;
        }
        else if(chars_rxed>0){
            sbus_data[chars_rxed]=ch;
            //printf("%02X ",ch);
            chars_rxed++;            
        }
        
        switch(chars_rxed){
            case 3:
                Chdata[0]=(sbus_data[1]|(sbus_data[2]<<8)&0x07ff);
                //printf("%04d ",data);
                break;
            case 4:
                Chdata[1]=(sbus_data[3]<<5|sbus_data[2]>>3)&0x07ff;
                //printf("%04d ",data);
                break;
            case 6:
                Chdata[2]=(sbus_data[3]>>6|sbus_data[4]<<2|sbus_data[5]<<10)&0x07ff;
                //printf("%04d ",data);
                break;
            case 7:
                Chdata[3]=(sbus_data[6]<<7|sbus_data[5]>>1)&0x07ff;
                //printf("%04d ",data);
                break;
            case 8:
                Chdata[4]=(sbus_data[7]<<4|sbus_data[6]>>4)&0x07ff;
                //printf("%04d ",data);
                break;
            case 10:
                Chdata[5]=(sbus_data[7]>>7|sbus_data[8]<<1|sbus_data[9]<<9)&0x07ff;
                //printf("%04d ",data);
                break;
        }

        if(chars_rxed==25){
            //printf("\n");
            chars_rxed=0;
        }
    }
}

int main() {
    stdio_init_all();
    init_serial();
    init_pwm();

    uint16_t duty;

    sleep_ms(5000);
    while(true){
      tight_loop_contents();
      duty=(float)(DUTYMAX-DUTYMIN)*(float)(Chdata[2]-CH3MIN)/(float)(CH3MAX-CH3MIN)+DUTYMIN;
      if (duty>DUTYMAX)duty=DUTYMAX;
      if (duty<DUTYMIN)duty=DUTYMIN;
      pwm_set_chan_level(slice_num, PWM_CHAN_A, duty);
      printf("%04d %04d %04d %04d %04d %04d \n",Chdata[0], Chdata[1], duty, Chdata[3], Chdata[4], Chdata[5]);
      sleep_ms(10);
    }
}


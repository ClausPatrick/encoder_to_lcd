#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/uart.h"

#include "u2u_source.h"
#include "u2u_source.c"
#include "u2u_aux.h"
#include "u2u_aux.c"
#include "u2uclientdef.h"


#define ONBOARD_LED 25
#define LED_0 18
#define LED_1 19
#define LED_2 20

#define OUTPUTNR 4
const int outputarray[] = {ONBOARD_LED, LED_0, LED_1, LED_2};

static bool tog_led = 0;
static char event_str[128];

//Pin definitions for the rotary encoders.
//inputarray are all pins configured as input. Order doesn't matter.
//edgedetectionarray are for Rside pins on encoders as well as push buttons. Order matters.
//leftsidearray are for Lside on encoders padded with nulls for buttons. Order matters.

//0, 1, 2, 3, 4, 5, -- 6, 7, -- <8, 9 uart1> -- 10, 11.
const int inputarray[] = {0, 1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13};
const int edgedetectarray[] = {4,  7, 1, 12, 5, 6, 3, 10}; // 0:3 for ROT and 4:7 for SW.
const int leftsidearray[] =   {2, 13, 0, 11, 0, 0, 0,  0}; // 0:3 for ROT, 4:7 can be ignored.

// Array rot_lookup for convinient fetching the other pin number and encoder index during irq callback.
int rot_lookup[20][3] = {0};
uint8_t  encoder_data[4] = {0};
uint8_t  button_data[4] = {0};
const int  dir_value[] = {-1, 1};

static int chars_rxed = 0;
static uint8_t encoder_data_change_flag = 1;

bool timer_callback(repeating_timer_t *rt);

void gpio_callback(uint gpio, uint32_t events){
    //gpio_put(LED_0, 1);
    //printf("irq: %d, %d\n", gpio, events);
    if (rot_lookup[gpio][1]==0) {
        //printf("button%d: %d\n", rot_lookup[gpio][0], gpio_get(gpio));
        button_data[rot_lookup[gpio][0]] = gpio_get(gpio);
    }else{
        int cwccw = gpio_get(gpio) ^ gpio_get(rot_lookup[gpio][1]);
        encoder_data[rot_lookup[gpio][0]] = (encoder_data[rot_lookup[gpio][0]] + dir_value[cwccw]);
        //if (encoder_data[rot_lookup[gpio][0]]<0){encoder_data[rot_lookup[gpio][0]]=0;}
        //if (encoder_data[rot_lookup[gpio][0]]>255){encoder_data[rot_lookup[gpio][0]]=255;}
    }
    encoder_data_change_flag = 1;
    printf("%d\n", gpio);
    gpio_put(LED_0, 0);
}

void io_setup(){
    int i;
    int r;
    for (i=0; i<OUTPUTNR; i++){
        gpio_init(outputarray[i]);
        gpio_set_dir(outputarray[i], GPIO_OUT);
        gpio_put(outputarray[i], 0);
    }

    for (i=0; i<12; i++){
            gpio_init(inputarray[i]);
            gpio_set_dir(inputarray[i], GPIO_IN);
    }

    for (i=0; i<8; i++){
            r = edgedetectarray[i];
            gpio_set_irq_enabled_with_callback(r, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
            rot_lookup[r][0] = i%4;
            rot_lookup[r][1] = leftsidearray[i];
    }
}

int main(){
    stdio_usb_init();
    io_setup();
    message_setup();
    gpio_put(LED_0, 1);
    repeating_timer_t timer;
    add_repeating_timer_ms(250, timer_callback, NULL, &timer);

    while (1){
        //printf("edcf: %d\n", encoder_data_change_flag);
        if (message_counter[0] > 0){
            printf("mc0\n");
            gpio_put(LED_1, 1);
            //printf("message counter 0 increased\n");
            print_messages(0);
            message_counter[0]--;
            message_handler(0);
       }
       if (message_counter[1] > 0){
            printf("mc1\n");
            gpio_put(LED_2, 1);
            //printf("message counter 1 increased\n");
            print_messages(1);
            message_counter[1]--;
            message_handler(1);
       }


       if (encoder_data_change_flag != 0){
           //gpio_put(LED_1, 1);
           for (int i=0; i<4; i++){
                printf(":%d", encoder_data[i]);
                printf(":%d\n", button_data[i]);
                //uart_putc(UART_ID, encoder_data[i]);
           }
           printf("\n");
           encoder_data_change_flag = 0;
       }
    }
    return 0;
}

bool timer_callback(repeating_timer_t *rt){
    //printf("yo\n");
    tog_led = 1-tog_led;
    gpio_put(ONBOARD_LED, tog_led);
    return true;
}

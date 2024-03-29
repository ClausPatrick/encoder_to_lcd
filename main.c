#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/uart.h"

extern "C" {
//#include "u2uclientdef.h"
#include "u2u.h"
}

#define ONBOARD_LED 25
#define LED_0 18
#define LED_1 19
#define LED_2 20

#define OUTPUTNR 4

const int   outputarray[]       = {ONBOARD_LED, LED_0, LED_1, LED_2};
static bool tog_led             = 0;
static char event_str[128];

//Pin definitions for the rotary encoders.
//inputarray are all pins configured as input. Order doesn't matter.
//edgedetectionarray are for Rside pins on encoders as well as push buttons. Order matters.
//leftsidearray are for Lside on encoders padded with nulls for buttons. Order matters.

//0, 1, 2, 3, 4, 5, -- 6, 7, -- <8, 9 uart1> -- 10, 11.
const int   inputarray[]        = {0, 1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13};
const int   edgedetectarray[]   = {2, 7,  0, 13, 5, 6, 3, 12}; // 0:3 for ROT and 4:7 for SW.
const int   leftsidearray[]     = {4, 11, 1, 10, 0, 0, 0,  0}; // 0:3 for ROT, 4:7 can be ignored.

// Array rot_lookup for convinient fetching the other pin number and encoder index during irq callback.
int         rot_lookup[20][3]   = {0};
uint8_t     encoder_data[4]     = {0};
uint8_t     button_data[4]      = {0};
const int   dir_value[]         = {1, -1};
static int  chars_rxed          = 0;

bool        timer_callback(repeating_timer_t *rt);
void        (*rot_but_functions[16])(uint);
volatile static uint8_t encoder_data_change_flag = 1;

void button_process_function(uint gpio){
    //button_data[rot_lookup[gpio][0]] = gpio_get(gpio);
    button_data[rot_lookup[gpio][0]] = 1 - button_data[rot_lookup[gpio][0]];
}

void rotary_process_function(uint gpio){
    int cwccw = gpio_get(gpio) ^ gpio_get(rot_lookup[gpio][1]);
    encoder_data[rot_lookup[gpio][0]] = (encoder_data[rot_lookup[gpio][0]] + dir_value[cwccw]);
    if (encoder_data[rot_lookup[gpio][0]]<0){encoder_data[rot_lookup[gpio][0]]=0;}
    if (encoder_data[rot_lookup[gpio][0]]>255){encoder_data[rot_lookup[gpio][0]]=255;}
}

void gpio_callback(uint gpio, uint32_t events){
    gpio_put(LED_0, 1);
    (*rot_but_functions[rot_lookup[gpio][1]])(gpio);
    encoder_data_change_flag = 1;
    gpio_put(LED_0, 0);
}

void io_setup(){
    int i, r;
    rot_but_functions[0] = &button_process_function;
    for (i=1; i<16; i++){
        rot_but_functions[i] = &rotary_process_function;
    }

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
        //gpio_set_irq_enabled_with_callback(r, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
        gpio_set_irq_enabled_with_callback(r, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
        rot_lookup[r][0] = i%4;
        rot_lookup[r][1] = leftsidearray[i];
    }
}

int main(){
    stdio_usb_init();
    io_setup();
    message_setup();
    gpio_put(LED_0, 0);
    repeating_timer_t timer;
    add_repeating_timer_ms(250, timer_callback, NULL, &timer);
    char for_arr[3][9] = {"self", "gen", "other"};
    int r, i;
    uint8_t chapter = 0;
    char str_[4];
    struct Message_out* inbound_message;
    struct Message_out* message_tx;
    struct Message_out message_txx;
    message_tx = &message_txx;
    int index = 0;
    int v;
    while (1){
        //printf("edcf: %d\n", encoder_data_change_flag);
       if (message_ready){
           gpio_put(LED_1, 1);
           inbound_message = message_processor();
           printf("got a message M: %s\n", inbound_message->Payload);
           gpio_put(LED_1, 0);
       }

       //r = message_processor();
       if (encoder_data_change_flag != 0){
           gpio_put(LED_0, 1);
           index = copy_str(message_tx->Receiver, "DEEPSPACE9", 0);
           message_tx->Receiver[index] = 0;
           index++;
           index = copy_str(message_tx->Topic, "SET ENCODER", 0);
           message_tx->Topic[index] = 0;
           int_to_ascii(str_, chapter, 3);
           chapter++;
           index = copy_str(message_tx->Chapter, str_, 0);
           message_tx->Chapter[index] = 0;
           index = 0;
           for (i=0; i<4; i++){
               int_to_ascii(str_, encoder_data[i], 3);
               index = copy_str(message_tx->Payload, str_, index);
               message_tx->Payload[index] = ',';
               index++;
               message_tx->Payload[index] = ' ';
               index++;
           }

           for (i=0; i<4; i++){
               int_to_ascii(str_, button_data[i], 3);
               index = copy_str(message_tx->Payload, str_, index);
               message_tx->Payload[index] = ',';
               index++;
               message_tx->Payload[index] = ' ';
               index++;
           }
           message_tx->Payload[index] = 0;
                //printf(":%d", encoder_data[i]);
                //printf(":%d\n", button_data[i]);
                //uart_putc(UART_ID, encoder_data[i]);
          // printf("Encoder Topic: %s\n", message_tx->Topic);
          // printf("Encoder Chapter: %s\n", message_tx->Chapter);
          // printf("Encoder data: %s\n", message_tx->Payload);
          send_message(message_tx);
          encoder_data_change_flag = 0;
           gpio_put(LED_0, 0);
       }
    }
    return 0;
}

bool timer_callback(repeating_timer_t *rt){
    //printf("mr: %d\n", message_ready);
    tog_led = 1-tog_led;
    gpio_put(ONBOARD_LED, tog_led);
    return true;
}

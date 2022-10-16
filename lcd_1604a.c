#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#define GPIO_ON 1
#define GPIO_OFF 0

#define LED_PIN 25


#define RS 26
#define E 22
#define DB4 21
#define DB5 20
#define DB6 19
#define DB7 18

#define RS_COMMAND 0
#define RS_DATA 1

#define LCD_FS 0x02 //Function set
#define LCD_DCO 0x28 //Display control
#define LCD_CD 0x02 //
#define LCD_DCL 0x01 //Display clear
#define LCD_MH 0x80 //Move home

#define UART_TX_PIN 4
#define UART_RX_PIN 5

#define UART_TX_RPI_PIN 12
#define UART_RX_RPI_PIN 13


#define BAUD_RATE 115200
#define UART_ID uart1

static int chars_rxed = 0;

const uint8_t datapins[] = {DB4, DB5, DB6, DB7};
const uint8_t display_layout[] = {0x00, 0x40, 0x10, 0x50};
const uint8_t outputpins[] = {RS, E, DB4, DB5, DB6, DB7};

//uint8_t rx_buffer[255];
uint8_t lcd_buffer[16*4];

#define DATA_FIELDS  4
uint8_t rx_data[DATA_FIELDS][16];
static uint8_t field_index = 0; // There are DATA_FIELDS nr of spots to store some data.
static uint8_t digit_index = 0; // Three digits for 000 to 255.
static uint8_t encoder_data_change_flag = 1;

void uart_irq_routine()
{
        gpio_put(LED_PIN, 1);
        int packet_counter = 0;
        int packet_size = 1;
        //uart_read_blocking(UART_ID, rx_buffer, packet_size);
        //uart_write_blocking(uart0, rx_buffer, packet_size);
        while (uart_is_readable(uart1))
        {
            uint8_t ch = uart_getc(UART_ID);
            switch (ch)
            {
                    case 58: // :
                            if (digit_index) // This accounts for when transmittion starts with ':' 
                            {
                                    for (int i=digit_index; i<3; i++)
                                    {
                                        rx_data[field_index][i] = 32;
                                    }
                            }
                            field_index = (field_index + 1) % DATA_FIELDS;
                            digit_index = 0;
                            break;
                    case 13 || 10: // LF, CR
                            field_index = 0;
                            digit_index = 0;
                            break;
                    default: // Todo, asserting value is numeric.
                            encoder_data_change_flag = 1;
                            rx_data[field_index][digit_index] = ch;
                            digit_index = (digit_index + 1) % 16;
                            break;
            }
            uart_putc(uart0, ch);
            //rx_buffer[chars_rxed] = ch;
            chars_rxed = chars_rxed + packet_size;
        }
        gpio_put(LED_PIN, 0);
}


void io_setup()
{
        uart_init(uart1, BAUD_RATE);
        uart_init(uart0, BAUD_RATE);
        gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(UART_TX_RPI_PIN, GPIO_FUNC_UART);
        gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
        gpio_set_function(UART_RX_RPI_PIN, GPIO_FUNC_UART); 

        uart_set_fifo_enabled(UART_ID, false);
        int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
        irq_set_exclusive_handler(UART_IRQ, uart_irq_routine);
        irq_set_enabled(UART_IRQ, true);
        uart_set_irq_enables(UART_ID, true, false);

        gpio_init(LED_PIN);
        gpio_set_dir(LED_PIN, GPIO_OUT);
        for (int c=0; c<6; c++)
        {
                gpio_init(outputpins[c]);
                gpio_set_dir(outputpins[c], GPIO_OUT);
                gpio_put(outputpins[c], 0);
        }
}

void lcd_send(uint8_t *buf, uint8_t len, uint8_t command_or_data)
{
        for (int c=0; c<len; c++)
        {
                uint8_t temp = buf[c];
                temp = temp >> 4;
                gpio_put(E, GPIO_ON);
                sleep_us(10);
                for (uint8_t d=0; d<4; d++)
                {
                        gpio_put(datapins[d], ((temp>>d) & 0b00000001));
                }
                gpio_put(RS, command_or_data);
                sleep_us(10);
                gpio_put(E, GPIO_OFF);
                sleep_us(20);
                
                temp = buf[c]; 
                gpio_put(E, GPIO_ON);
                sleep_us(10);
                for (uint8_t d=0; d<4; d++)
                {
                        gpio_put(datapins[d], ((temp>>d) & 0b00000001));
                }
                gpio_put(RS, command_or_data);
                sleep_us(10);
                gpio_put(E, GPIO_OFF);
                sleep_us(20);

                for (uint8_t z=0; z<4; z++)
                {
                        gpio_put(datapins[z], 0);
                }
        }
}

void lcd_init()
{
        //Initialisation commands:
        //-Function set (4b/2L/5x7)
        //-Display control
        //-Display clear
        //-Return home
        uint8_t initialisation_commands[] = {0x02, 0x02, 0x28, 0x0E, 0x01, 0x80};
        //Initialisation phase is sandwiched with delay times that come from 
        //the datasheet. Found that when adding 100 each phase yields more reliable operation.
        int initialisation_delay_times[] = {40, 40, 40, 1600, 1600, 0};
        for (int i=0; i<6; i++)
        {
                lcd_send(&initialisation_commands[i], 1, RS_COMMAND );
                sleep_us(300+initialisation_delay_times[i]);
        }
        sleep_ms(100);
        return;
}


void lcd_fill_lines(uint8_t *buf)
{
        for (int lines=0; lines<4; lines++)
        {
                uint8_t adr = 0b10000000 | display_layout[lines];
                lcd_send(&adr, 1, RS_COMMAND);
                lcd_send(buf+(16*lines), 16, RS_DATA);
                //lcd_send(&buf[16*lines], 16, 1);
        }
}


int main()
{
        /*
        for (int i=0; i<255; i++)
        {
                rx_buffer[i] = 20;
        }
        */

        for (int i=0; i<(16*4); i++)
        {
                lcd_buffer[i] = 20;
        }
        for (int i=0; i<DATA_FIELDS; i++)
        {
                for (int j=0; j<16; j++)
                {
                        rx_data[i][j] = 20;
                }

        }

        io_setup();
        lcd_init();

        sleep_ms(10);

        lcd_fill_lines(lcd_buffer);
        sleep_ms(1000);

        while (true)
        {
                if (chars_rxed>16)
                {
                    //printf("Mwh ch_rxed: %d\n", chars_rxed);
                    char ec[49];
                    sprintf(ec, "chrx: %d,   | ", chars_rxed);
                    uart_puts(uart0, ec);
                    uart_putc(UART_ID, 13);
                    uart_putc(UART_ID, 10);
                    /*
                    for (int i=0; i<chars_rxed; i++)
                    {   
                            lcd_buffer[i] = rx_buffer[i];
                    }
                    for (int i=chars_rxed; i<(16*4); i++)
                    {
                            lcd_buffer[i] = 20;
                    }   
                    */

                    if (encoder_data_change_flag)
                    {
                            encoder_data_change_flag = 0;
                            for (int i=0; i<4; i++)
                            {
                                    for (int j=0; j<3; j++) // 3 is lenght allocated to store value in ascii format.
                                    {
                                        lcd_buffer[(i*16)+j] = rx_data[i][j];
                                    }
                                    for (int j=3; j<16; j++) 
                                    {
                                        lcd_buffer[(i*16)+j] = 32;
                                    }
                            lcd_fill_lines(lcd_buffer);
                            sleep_ms(1);
                            }
                    }

                    chars_rxed = 0;
                }
        }
        return 0;
}


/*
 *



                uint8_t ch = uart_getc(UART_ID);
                chars_rxed++;
                //printf("%d\n", ch);
                if (ch>47 && ch<57)
                {
                    uart_putc(uart0, ch);
                    rx_buffer[packet_counter] = ch;
                    packet_counter++;
                }
                */

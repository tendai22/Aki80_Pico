//
// Aki80_Pico ... A Pico rom bootloader and uart emulation for Aki-80 singleboard computer
// Norihiro Kumagai 2025/5/25
//

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/uart.h"

#include "blink.pio.h"

// Z80 Pins
// D0-D7: GPIO0-7
// A7,6,5: GPIO10,9,8
// UART0TX,RX: GPIO12,13
#define MREQ_Pin    14
#define RFSH_Pin    15
#define BUSAK_Pin   16
#define IORQ_Pin    17
#define RD_Pin      18
#define A0_Pin      19
#define RAMCE2_Pin  20
#define RAMWE_Pin   21
#define RESET_Pin   26
#define WAIT_Pin    27
#define BUSRQ_Pin   28

#define TEST_Pin    22

#define TOGGLE() do {    gpio_xor_mask(((uint32_t)1)<<TEST_Pin); } while(0)

void sram_control_forever(PIO pio, uint sm, uint offset) {
    sram_control_program_init(pio, sm, offset);
}

void databus_control_forever(PIO pio, uint sm, uint offset) {
    databus_control_program_init(pio, sm, offset);
}

// UART defines
// By default the stdout UART is `uart0`, so we will use the second one
#define UART_ID uart0
#define BAUD_RATE 115200

// Use pins 12 and 13 for UART0
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define UART_TX_PIN 12
#define UART_RX_PIN 13

void gpio_out_init(uint gpio, bool value) {
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, value);
    gpio_set_function(gpio, GPIO_FUNC_SIO);
}

static uint8_t ram16[16] = {
    0x76
};

//
// bootloader ...
//

static uint8_t z80_sram_cycle(int pindir, uint8_t instruction, uint8_t wr_data)
{
    uint8_t mask = pindir ? 0xff : 0;   // pindir: true: OUT, false: IN
    uint8_t data = 0;
    TOGGLE();
    while(((gpio = gpio_get_all()) & (1<<MREQ_Pin)));    // wait for MREQ cycle start
    if ((gpio & (1<<RFSH_Pin)) == 0) {
        // Refresh cycle ... ignored, possible never happens
    } else if (gpio & (1<<RD_Pin)) {
        // WR cycle ... enable SRAM and WE asserted
        gpio_set_dir_masked(0xff, 0xff);    // D0-D7 output
        gpio_put_masked(0xff, wr_data);
        gpio_put(RAMCE2_Pin, false);
        gpio_put(WE_Pin, false);
        sleep_us(1);
        gpio_put(WE_Pin, true);
        gpio_put(RAMCE2_Pin, high);
    }
    TOGGLE();
    // post process
    if ((gpio & (1<<RD_Pin)) == 0) {
        // RD cycle, put instruction on D0-D7
        gpio_set_dir_masked(0xff, 0xff);    // D0-D7 output
        gpio_put_masked(0xff, instruction);
    }
    gpio_put(BUSRQ_Pin, false);         // a BUSRQ trap is prepared
    gpio_put(WAIT_Pin, true);           // restart Z90
    while ((gpio_get_all() & (1<<BUSRQ_Pin)));  // wait for BUSAK assert 
    gpio_set_dir_masked(0xff, 0);       // D0-D7 input
    gpio_put(WAIT_Pin, false);          // for next cycle
    gpio_put(BUSRQ_Pin, true);          // restart Z80
    return data;
}

void boot(uint16_t start, uint8_t *buf, int length)
{
    uint8_t data;
    gpio_out_init(OE_Pin, true);
    gpio_out_init(WE_Pin, true);

    gpio_put(WAIT_Pin, false);
    gpio_put(RESET_Pin, true);  // start it
    z80_sram_cycle(-1, 0xc3, 0);
    z80_sram_cycle(-1, (start)&0xff, 0);
    z80_sram_cycle(-1, (start>>8)&0xff, 0);
    for (int i = 0; i < sizeof ram16; ++i) {
        z80_sram_cycle(1, 0, ram16[i]);
    }
    // read sram
    gpio_put(RESET_Pin, false);
    sleep_ms(1);
    gpio_put(WAIT_Pin, false);
    gpio_put(RESET_Pin, true);  // restart it
    z80_sram_cycle(-1, 0xc3, 0);
    z80_sram_cycle(-1, (start)&0xff, 0);
    z80_sram_cycle(-1, (start>>8)&0xff, 0);
    for (int i = 0; i < sizeof ram16; ++i) {
        data = z80_sram_cycle(0, 0, 0);
        if (i % 8 == 0)
            printf("%04X ", start + i);
        printf("%02X ", data);
        if (i % 8 == 7)
            printf("\n");
    }
    gpio_put(RESET_Pin, false);  // reset again
}

int main()
{
    stdio_init_all();
    // Set up our UART
    uart_init(UART_ID, BAUD_RATE);
    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // GPIO Out
    gpio_out_init(WAIT_Pin, true);
    gpio_out_init(RESET_Pin, false);
    gpio_out_init(BUSRQ_Pin, true);
    //gpio_out_init(INT_Pin, false);      // INT Pin has an inverter, so negate signal is needed
    gpio_out_init(OE_Pin, true);
    gpio_out_init(WE_Pin, true);

    gpio_out_init(TEST_Pin, true);
    TOGGLE();
    TOGGLE();
    //TOGGLE();

    // GPIO In
    // MREQ, RFSH are covered by PIO
    //
    gpio_init_mask(0x7ff);     // D0-D7,A5,6,7 input 
    gpio_set_dir_masked(0xff, 0);   // D0-D7 input
    gpio_init(BUSAK_Pin);
    gpio_init(RFSH_Pin);
    // MREQ, IORQ, RD may be Hi-Z, so weak pullup
    gpio_init(MREQ_Pin);
    gpio_set_pulls(MREQ_Pin, true, false);
    gpio_init(IORQ_Pin);
    gpio_set_pulls(IORQ_Pin, true, false);
    gpio_init(RD_Pin);
    gpio_set_pulls(RD_Pin, true, false);

    // pio_set_gpio_base should be invoked before pio_add_program
    uint offset1;
    offset1 = pio_add_program(pio0, &databus_control_program);
    printf("databus_control_program at %d\n", offset1);
    databus_control_pin_forever(pio0, 0, offset1);

    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART
    
    // Send out a string, with CR/LF conversions
    uart_puts(UART_ID, " Hello, UART!\r\n");

    // test program
    // start CPU and examine signal transition
    gpio_put(WAIT_Pin, true);
    gpio_put(RESET_Pin, true);

    // infinite loop
    while(true);

    // boot ... expand Z80 program to SRAM
    boot(0, &ram16[0], sizeof ram16);

    // setup sram_control PIO sm
    offset1 = pio_add_program(pio_clock, &sram_control_program);
    printf("sram_control_program at %d\n", offset1);
    sram_control_forever(pio_clock, 1, offset1);
    pio_sm_set_enabled(pio_clock, 1, true);
    // restart CPU
    gpio_put(WAIT_Pin, true);
    gpio_put(RESET_Pin, true);

    // For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart

    int count = 3;    
    while (true) {
        data = pio_sm_get_blocking(pio0, 0);    // wait for MREQ cycle start
        status = gpio_get_all() >> 14;
        // bit0: MREQ
        // bit1: RFSH
        // bit2: BUSAK
        // bit3: IORQ
        // bit4: RD
        // bit5: A0
        if ((status & (1<<4)) == 0) {   // RD cycle
            gpio_set_dir_masked(0xff, 0xff);    // D0-D7 output
            gpio_put_masked(0xff, 0);   // put NOP
            pio_sm_put(pio0, 0, 0);
            pio_sm_get_blocking(pio0, 0);   // wait for MREQ cycle end
            gpio_set_dir_masked(0xff, 0);   // D0-D7 input
        }

    }
}

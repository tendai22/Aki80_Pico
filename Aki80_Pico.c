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
#define RAMCS2_Pin  20
#define RAMWE_Pin   21
#define RESET_Pin   26
#define WAIT_Pin    27
#define BUSRQ_Pin   28

#define TEST_Pin    22

#include "basic/basic.h"

#define TOGGLE() do {    gpio_xor_mask(((uint32_t)1)<<TEST_Pin); } while(0)

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

static uint8_t ram16[4] = {
    0x76
};

//
// bootloader ...
//
#define INST_MODE 0
#define READ_MODE 1
#define WRITE_MODE 2

static int verbose = 0;

static uint8_t z80_machine_cycle(int mode, uint8_t data)
{
    uint32_t gpio;
    TOGGLE();
    pio_sm_get_blocking(pio0, 0);    // wait for access event occurs
    gpio = gpio_get_all();
    
    if (mode == INST_MODE) {
        // M1 cycle
        gpio_set_dir_masked(0xff, 0xff);    // D0-D7 output
        gpio_put_masked(0xff, data);
        if (verbose) printf("I%02X\n", data);
    } else if (mode == READ_MODE) {
        // Memory Read cycle
        gpio_set_dir_masked(0xff, 0);       // D0-D7 input
        gpio_put(RAMCS2_Pin, false);        // SRAM enable
        sleep_us(1);
        data = gpio_get_all() & 0xff;       // read sram data
        if (verbose) printf("R%02X\n", data);
    } else if (mode == WRITE_MODE) {
        // Memry Write cycle
        gpio_set_dir_masked(0xff, 0);       // D0-D7 input
        // no databus output, let Z80 to write SRAM
        gpio_put(RAMWE_Pin, false);
        gpio_put(RAMCS2_Pin, true);
        sleep_us(1);        // wait for SRAM data settled down
        gpio_put(RAMWE_Pin, true);
        gpio_put(RAMCS2_Pin, true);
        data = gpio_get_all() & 0xff;       // read databus data
        if (verbose) printf("W%02X\n", data);
    }
    pio_sm_put(pio0, 0, 0);         // release WAIT machine
    pio_sm_get_blocking(pio0, 0);   // wait for MREQ negated
    gpio_set_dir_masked(0xff, 0);           // D0-D7 Hi-Z
    pio_sm_put(pio0, 0, 0);
    TOGGLE();
    return data;
}

void z80_set_addr(uint16_t addr)
{
    gpio_put(RESET_Pin, false);     // reset Z80
    sleep_us(1);
    gpio_put(RESET_Pin, true);      // restart
    z80_machine_cycle(INST_MODE, 0x21);     // LD HL,addr
    z80_machine_cycle(INST_MODE, addr&0xff);
    z80_machine_cycle(INST_MODE, (addr>>8)&0xff);
}

void z80_write_byte(uint8_t data)
{
    z80_machine_cycle(INST_MODE, 0x3e);     // LD A,data
    z80_machine_cycle(INST_MODE, data);
    z80_machine_cycle(INST_MODE, 0x77);     // LD (HL),A
    z80_machine_cycle(WRITE_MODE, 0);
    z80_machine_cycle(INST_MODE, 0x23);     // INC HL
}

uint8_t z80_read_byte(void)
{
    uint8_t data;
    z80_machine_cycle(INST_MODE, 0x7e);     // LD A,(HL)
    data = z80_machine_cycle(READ_MODE, 0);
    z80_machine_cycle(INST_MODE, 0x23);     // INC HL
    return data;
}

void boot(uint16_t start, uint8_t *buf, int length)
{
    uint8_t data;

    printf("boot: start = %04X, length = %d\n", start, length);
    z80_set_addr(start);
    for (int i = 0; i < length; ++i) {
        z80_write_byte(buf[i]);
    }
    z80_set_addr(start);
    for (int i = 0; i < length; ++i) {
        data = z80_read_byte();
        if ((i % 8) == 0)
            printf("%04X ", start + i);
        printf("%02X ", data);
        if ((i % 8) == 7)
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
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));

    uart_puts(UART_ID, " Hello, UART!\r\n");
    printf("Hello, printf\n");

    // test pin, scratch its initial point
    gpio_out_init(TEST_Pin, true);
    TOGGLE();
    TOGGLE();

    // GPIO Out
    gpio_out_init(WAIT_Pin, true);
    gpio_out_init(RESET_Pin, false);
    gpio_out_init(BUSRQ_Pin, true);
    gpio_out_init(RAMCS2_Pin, false);
    gpio_out_init(RAMWE_Pin, true);


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
    databus_control_forever(pio0, 0, offset1);

    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART
    
    // Send out a string, with CR/LF conversions

    // test program 1: Z80 Free Run
    // start CPU and examine signal transition
    pio_sm_set_enabled(pio0, 0, true);
    sleep_us(10);
    TOGGLE();
    TOGGLE();

    pio_sm_clear_fifos(pio0, 0);
    gpio_put(WAIT_Pin, true);
    gpio_put(RESET_Pin, true);


    // boot ... expand Z80 program to SRAM
    boot(0, &ram16[0], sizeof ram16);
    // infinite loop
    while(true);
}

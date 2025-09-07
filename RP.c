#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#define SPI_PORT spi0
#define SPI_MISO_PIN 16
#define SPI_MOSI_PIN 19
#define SPI_SCLK_PIN 18
#define SPI_CSN_PIN 17

#define CC1101_PARTNUM 0x30
#define READ_SINGLE    0x80

int main() {
    stdio_init_all();
    sleep_ms(2500); // Wait for user to connect serial monitor

    printf("--- Barebones CC1101 SPI Test ---\n");
    printf("This test will attempt to read the PARTNUM register from the CC1101.\n");
    printf("Please ensure you are viewing this message over the PICO'S USB VIRTUAL COM PORT.\n\n");

    spi_init(SPI_PORT, 1000 * 1000); // Use a safe, slow 1MHz speed
    gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_SCLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);

    gpio_init(SPI_CSN_PIN);
    gpio_set_dir(SPI_CSN_PIN, GPIO_OUT);
    gpio_put(SPI_CSN_PIN, 1);

    printf("SPI Initialized on port spi0.\n");
    printf("Pins: MISO=%d, MOSI=%d, SCLK=%d, CSN=%d\n\n", SPI_MISO_PIN, SPI_MOSI_PIN, SPI_SCLK_PIN, SPI_CSN_PIN);

    uint8_t addr = CC1101_PARTNUM | READ_SINGLE;
    uint8_t partnum_val;

    // Manually control CSN for clarity
    gpio_put(SPI_CSN_PIN, 0);
    sleep_us(10);
    spi_write_blocking(SPI_PORT, &addr, 1);
    spi_read_blocking(SPI_PORT, 0, &partnum_val, 1);
    sleep_us(10);
    gpio_put(SPI_CSN_PIN, 1);

    printf("Attempted to read PARTNUM register (command byte 0xB0).\n");
    printf("Value received from MISO: 0x%02X\n\n", partnum_val);

    if (partnum_val == 0x00) {
        printf("Result: SUCCESS!\n");
        printf("The CC1101 PARTNUM is 0x00 as expected. Communication is working.\n");
    } else {
        printf("Result: FAILED.\n");
        printf("Expected 0x00, but got 0x%02X.\n", partnum_val);
        printf("This confirms a problem with the SPI communication. Please double-check wiring, especially the MISO pin.\n");
    }

    printf("\nTest complete. The device will now do nothing.\n");

    while(1) {
        tight_loop_contents();
    }

    return 0;
}

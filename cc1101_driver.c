#include "cc1101_driver.h"
#include "cc1101_constants.h"
#include "hardware/gpio.h"
#include <stdio.h>

// --- Local helper functions ---

/**
 * @brief Select the CC1101 chip by pulling CS low.
 */
static inline void cs_select() {
    gpio_put(CC1101_PIN_CS, 0);
}

/**
 * @brief Deselect the CC1101 chip by pulling CS high.
 */
static inline void cs_deselect() {
    gpio_put(CC1101_PIN_CS, 1);
}

// --- Public function implementations ---

void cc1101_init() {
    // Initialize SPI
    spi_init(CC1101_SPI_PORT, 1000 * 1000); // Initialize at 1 MHz
    gpio_set_function(CC1101_PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(CC1101_PIN_SCLK, GPIO_FUNC_SPI);
    gpio_set_function(CC1101_PIN_MOSI, GPIO_FUNC_SPI);

    // Initialize Chip Select pin
    gpio_init(CC1101_PIN_CS);
    gpio_set_dir(CC1101_PIN_CS, GPIO_OUT);
    gpio_put(CC1101_PIN_CS, 1); // Deselect chip

    // Initialize GDO pins if needed (example for GDO0 as input)
    gpio_init(CC1101_PIN_GDO0);
    gpio_set_dir(CC1101_PIN_GDO0, GPIO_IN);
    gpio_pull_down(CC1101_PIN_GDO0); // Use pull-down

    printf("CC1101 SPI and GPIO initialized.\n");
}

void cc1101_strobe(uint8_t strobe) {
    cs_select();
    spi_write_blocking(CC1101_SPI_PORT, &strobe, 1);
    cs_deselect();
}

void cc1101_write_reg(uint8_t reg_addr, uint8_t value) {
    uint8_t buffer[2] = {reg_addr, value};
    cs_select();
    spi_write_blocking(CC1101_SPI_PORT, buffer, 2);
    cs_deselect();
}

uint8_t cc1101_read_reg(uint8_t reg_addr) {
    uint8_t addr = reg_addr | CC1101_READ_SINGLE;
    uint8_t value;
    cs_select();
    spi_write_blocking(CC1101_SPI_PORT, &addr, 1);
    spi_read_blocking(CC1101_SPI_PORT, 0, &value, 1); // Read value
    cs_deselect();
    return value;
}

uint8_t cc1101_read_status_reg(uint8_t reg_addr) {
    uint8_t addr = reg_addr | CC1101_READ_BURST;
    uint8_t status;
    cs_select();
    spi_write_blocking(CC1101_SPI_PORT, &addr, 1);
    spi_read_blocking(CC1101_SPI_PORT, 0, &status, 1);
    cs_deselect();
    return status;
}

void cc1101_write_burst_reg(uint8_t reg_addr, const uint8_t *buffer, uint8_t count) {
    uint8_t addr = reg_addr | CC1101_WRITE_BURST;
    cs_select();
    spi_write_blocking(CC1101_SPI_PORT, &addr, 1);
    spi_write_blocking(CC1101_SPI_PORT, buffer, count);
    cs_deselect();
}

void cc1101_read_burst_reg(uint8_t reg_addr, uint8_t *buffer, uint8_t count) {
    uint8_t addr = reg_addr | CC1101_READ_BURST;
    cs_select();
    spi_write_blocking(CC1101_SPI_PORT, &addr, 1);
    spi_read_blocking(CC1101_SPI_PORT, 0, buffer, count);
    cs_deselect();
}

void cc1101_reset() {
    cs_deselect();
    sleep_us(10);
    cs_select();
    sleep_us(10);
    cs_deselect();
    sleep_us(45);
    cc1101_strobe(CC1101_SRES);
    sleep_ms(1);
    printf("CC1101 Reset.\n");
}

// Basic configuration for 433.92 MHz OOK/ASK
void cc1101_configure() {
    cc1101_reset();

    // Set frequency to 433.92 MHz
    cc1101_write_reg(CC1101_FREQ2, 0x10);
    cc1101_write_reg(CC1101_FREQ1, 0xB0);
    cc1101_write_reg(CC1101_FREQ0, 0x71);

    // Modem configuration for OOK/ASK
    // Set widest possible RX filter bandwidth to catch signals that might be off-frequency
    cc1101_write_reg(CC1101_MDMCFG4, 0x08); // RX filter BW = 812 KHz (widest setting)
    cc1101_write_reg(CC1101_MDMCFG3, 0x21); // Data rate = 2.4 kBaud
    // Set to ASK/OOK, No preamble/sync
    cc1101_write_reg(CC1101_MDMCFG2, 0x30);
    cc1101_write_reg(CC1101_MDMCFG1, 0x22); // No FEC
    cc1101_write_reg(CC1101_MDMCFG0, 0xF8); // Channel spacing = 200 kHz

    // Disable all packet handling features for raw mode
    cc1101_write_reg(CC1101_PKTCTRL0, 0x00);
    // Set GDO0 to output the raw, demodulated serial data stream
    cc1101_write_reg(CC1101_IOCFG0, 0x0D);

    // Other settings
    cc1101_write_reg(CC1101_DEVIATN, 0x15);
    cc1101_write_reg(CC1101_MCSM1, 0x0C);    // Stay in RX mode after packet reception
    cc1101_write_reg(CC1101_MCSM0, 0x18);    // Auto-calibrate on IDLE -> RX/TX transition
    cc1101_write_reg(CC1101_FOCCFG, 0x16);
    // Use recommended AGC settings for ASK/OOK signals to improve adaptability
    cc1101_write_reg(CC1101_AGCCTRL2, 0x07);
    cc1101_write_reg(CC1101_AGCCTRL1, 0x00);
    cc1101_write_reg(CC1101_AGCCTRL0, 0xB2);
    cc1101_write_reg(CC1101_FREND1, 0x56);
    cc1101_write_reg(CC1101_FSCAL3, 0xE9);
    cc1101_write_reg(CC1101_FSCAL2, 0x2A);
    cc1101_write_reg(CC1101_FSCAL1, 0x00);
    cc1101_write_reg(CC1101_FSCAL0, 0x1F);

    // PA Table for Tx Power
    uint8_t pa_table[8] = {0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cc1101_write_burst_reg(CC1101_PATABLE, pa_table, 8);

    printf("CC1101 Configured for 433.92MHz ASK.\n");

    // Check version and part number
    uint8_t version = cc1101_read_status_reg(CC1101_VERSION);
    uint8_t partnum = cc1101_read_status_reg(CC1101_PARTNUM);
    printf("CC1101 Partnum: 0x%02X, Version: 0x%02X\n", partnum, version);
}

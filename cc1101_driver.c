#include "cc1101_driver.h"
#include <stdio.h>

// Helper function to select the CC1101
static inline void cs_select() {
    asm volatile("nop \n nop \n nop");
    gpio_put(PIN_CS, 0);
    asm volatile("nop \n nop \n nop");
}

// Helper function to deselect the CC1101
static inline void cs_deselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(PIN_CS, 1);
    asm volatile("nop \n nop \n nop");
}

// Send a command strobe to the CC1101
void cc1101_strobe(uint8_t strobe) {
    cs_select();
    spi_write_blocking(SPI_PORT, &strobe, 1);
    cs_deselect();
}

// Write a single register on the CC1101
void cc1101_write_reg(uint8_t addr, uint8_t value) {
    uint8_t data[2] = {addr, value};
    cs_select();
    spi_write_blocking(SPI_PORT, data, 2);
    cs_deselect();
}

// Read a single register from the CC1101
uint8_t cc1101_read_reg(uint8_t addr) {
    uint8_t value = 0;
    addr |= READ_SINGLE;
    cs_select();
    spi_write_blocking(SPI_PORT, &addr, 1);
    spi_read_blocking(SPI_PORT, 0, &value, 1);
    cs_deselect();
    return value;
}

// Write multiple registers (burst)
void cc1101_write_burst(uint8_t addr, uint8_t *data, uint8_t len) {
    addr |= WRITE_BURST;
    cs_select();
    spi_write_blocking(SPI_PORT, &addr, 1);
    spi_write_blocking(SPI_PORT, data, len);
    cs_deselect();
}

// Read multiple registers (burst)
void cc1101_read_burst(uint8_t addr, uint8_t *data, uint8_t len) {
    addr |= READ_BURST;
    cs_select();
    spi_write_blocking(SPI_PORT, &addr, 1);
    spi_read_blocking(SPI_PORT, 0, data, len);
    cs_deselect();
}


// Reset the CC1101 chip
void cc1101_reset() {
    cs_select();
    sleep_us(10);
    cs_deselect();
    sleep_us(40);
    cc1101_strobe(SRES);
    sleep_ms(1);
}

// Set the carrier frequency
void cc1101_set_freq(uint32_t freq) {
    uint64_t f = (uint64_t)freq << 16;
    uint32_t freq_reg = f / 26000000; // 26MHz crystal
    cc1101_write_reg(FREQ2, (freq_reg >> 16) & 0xFF);
    cc1101_write_reg(FREQ1, (freq_reg >> 8) & 0xFF);
    cc1101_write_reg(FREQ0, freq_reg & 0xFF);
}


// Initialize the CC1101 and SPI
void cc1101_init() {
    // Initialize SPI
    spi_init(SPI_PORT, 5 * 1000 * 1000); // 5MHz
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // Initialize CS pin
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    // Initialize GDO0 pin (for capture)
    gpio_init(PIN_GDO0);
    gpio_set_dir(PIN_GDO0, GPIO_IN);

    // Initialize GDO2 pin (for transmit)
    // This will be controlled by a PIO state machine,
    // but we can initialize it here.
    gpio_init(PIN_GDO2);
    gpio_set_dir(PIN_GDO2, GPIO_OUT);
    gpio_put(PIN_GDO2, 0);

    // Reset the chip
    cc1101_reset();

    // Configuration for receiving raw data
    // This configuration is for OOK/ASK modulation.
    // It's a starting point and may need tuning.
    cc1101_write_reg(IOCFG0, 0x06);   // GDO0 asserts when sync word has been sent/received, and de-asserts at the end of the packet.
    cc1101_write_reg(PKTCTRL0, 0x32); // Asynchronous serial mode, no CRC
    cc1101_write_reg(MDMCFG2, 0x00);  // 2-FSK, no manchester, no preamble/sync
    cc1101_write_reg(MDMCFG1, 0x00);  // No FEC
    cc1101_write_reg(DEVIATN, 0x15);  // Deviation
    cc1101_write_reg(MCSM1, 0x0F);    // Always go to IDLE after RX/TX
    cc1101_write_reg(MCSM0, 0x18);    // Auto-calibrate from IDLE to RX/TX
    cc1101_write_reg(FOCCFG, 0x16);
    cc1101_write_reg(AGCCTRL2, 0x43); // OOK/ASK settings
    cc1101_write_reg(AGCCTRL1, 0x40);
    cc1101_write_reg(AGCCTRL0, 0x91);
    cc1101_write_reg(FREND1, 0x56);
    cc1101_write_reg(FREND0, 0x11);
    cc1101_write_reg(FSCAL3, 0xE9);
    cc1101_write_reg(FSCAL2, 0x2A);
    cc1101_write_reg(FSCAL1, 0x00);
    cc1101_write_reg(FSCAL0, 0x1F);
    cc1101_write_reg(TEST2, 0x81);
    cc1101_write_reg(TEST1, 0x35);
    cc1101_write_reg(TEST0, 0x09);
    cc1101_write_reg(PKTCTRL0, 0x32); // Asynchronous serial mode
    cc1101_write_reg(MDMCFG2, 0x03);  // OOK/ASK modulation, no manchester, no preamble/sync
    cc1101_write_reg(IOCFG2, 0x29);   // default

    // Set frequency to 433.92 MHz
    cc1101_set_freq(433920000);

    printf("CC1101 Initialized.\n");
    uint8_t version = cc1101_read_reg(VERSION);
    uint8_t partnum = cc1101_read_reg(PARTNUM);
    printf("Partnum: 0x%02X, Version: 0x%02X\n", partnum, version);
}

#ifndef _CC1101_DRIVER_H_
#define _CC1101_DRIVER_H_

#include "pico/stdlib.h"
#include "hardware/spi.h"

// Define the SPI and GPIO pins to be used for the CC1101
// NOTE: This assumes a certain wiring layout.
// Make sure your hardware matches these definitions.
#define CC1101_SPI_PORT spi0
#define CC1101_PIN_MISO 16
#define CC1101_PIN_CS   17
#define CC1101_PIN_SCLK 18
#define CC1101_PIN_MOSI 19
#define CC1101_PIN_GDO0 20 // Used for packet reception notification
#define CC1101_PIN_GDO2 21 // Optional, can be used for other notifications

// Function Prototypes

/**
 * @brief Initializes the SPI interface and GPIO pins for the CC1101.
 */
void cc1101_init();

/**
 * @brief Sends a command strobe to the CC1101.
 * @param strobe The command strobe to send (e.g., CC1101_SRES, CC1101_SRX).
 */
void cc1101_strobe(uint8_t strobe);

/**
 * @brief Writes a single byte to a CC1101 configuration register.
 * @param reg_addr The address of the register to write to.
 * @param value The byte value to write.
 */
void cc1101_write_reg(uint8_t reg_addr, uint8_t value);

/**
 * @brief Reads a single byte from a CC1101 configuration or status register.
 * @param reg_addr The address of the register to read from.
 * @return The byte value read from the register.
 */
uint8_t cc1101_read_reg(uint8_t reg_addr);

/**
 * @brief Reads a status register. This is different from a normal read
 *        because the burst bit is used to access status registers.
 * @param reg_addr The address of the status register.
 * @return The status byte.
 */
uint8_t cc1101_read_status_reg(uint8_t reg_addr);


/**
 * @brief Writes multiple bytes to the CC1101 in a burst.
 * @param reg_addr The starting register address.
 * @param buffer Pointer to the data to write.
 * @param count The number of bytes to write.
 */
void cc1101_write_burst_reg(uint8_t reg_addr, const uint8_t *buffer, uint8_t count);


/**
 * @brief Reads multiple bytes from the CC1101 in a burst.
 * @param reg_addr The starting register address.
 * @param buffer Pointer to a buffer where the read data will be stored.
 * @param count The number of bytes to read.
 */
void cc1101_read_burst_reg(uint8_t reg_addr, uint8_t *buffer, uint8_t count);

/**
 * @brief Resets the CC1101 chip.
 */
void cc1101_reset();

/**
 * @brief Configures the CC1101 with basic settings for 433MHz operation.
 *        This is a starting point and may need tuning.
 */
void cc1101_configure();

/**
 * @brief Sets the carrier frequency for the CC1101.
 * @param freq_khz The desired frequency in kilohertz (e.g., 433920 for 433.92 MHz).
 */
void cc1101_set_frequency(uint32_t freq_khz);


#endif // _CC1101_DRIVER_H_

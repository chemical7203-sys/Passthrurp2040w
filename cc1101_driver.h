#ifndef CC1101_DRIVER_H
#define CC1101_DRIVER_H

#include "pico/stdlib.h"
#include "hardware/spi.h"

// SPI Pin definitions
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19

// GDO Pin definitions
#define PIN_GDO0 20 // Used for signal input (capture)
#define PIN_GDO2 21 // Used for signal output (transmit)

// CC1101 SPI Command Types
#define WRITE_BURST     0x40
#define READ_SINGLE     0x80
#define READ_BURST      0xC0

// Command Strobes
#define SRES            0x30        // Reset
#define SFSTXON         0x31        // Enable and calibrate frequency synthesizer
#define SXOFF           0x32        // Turn off crystal oscillator.
#define SCAL            0x33        // Calibrate frequency synthesizer and turn it off
#define SRX             0x34        // Enable RX
#define STX             0x35        // Enable TX
#define SIDLE           0x36        // Exit RX / TX
#define SFTX            0x3B        // Flush the TX FIFO buffer.
#define SFRX            0x3A        // Flush the RX FIFO buffer.
#define SNOP            0x3D        // No operation.

// CC1101 Registers
#define IOCFG2          0x00        // GDO2 output pin configuration
#define IOCFG1          0x01        // GDO1 output pin configuration
#define IOCFG0          0x02        // GDO0 output pin configuration
#define FIFOTHR         0x03        // RX FIFO and TX FIFO thresholds
#define SYNC1           0x04        // Sync word, high byte
#define SYNC0           0x05        // Sync word, low byte
#define PKTLEN          0x06        // Packet length
#define PKTCTRL1        0x07        // Packet automation control
#define PKTCTRL0        0x08        // Packet automation control
#define ADDR            0x09        // Device address
#define CHANNR          0x0A        // Channel number
#define FSCTRL1         0x0B        // Frequency synthesizer control
#define FSCTRL0         0x0C        // Frequency synthesizer control
#define FREQ2           0x0D        // Frequency control word, high byte
#define FREQ1           0x0E        // Frequency control word, middle byte
#define FREQ0           0x0F        // Frequency control word, low byte
#define MDMCFG4         0x10        // Modem configuration
#define MDMCFG3         0x11        // Modem configuration
#define MDMCFG2         0x12        // Modem configuration
#define MDMCFG1         0x13        // Modem configuration
#define MDMCFG0         0x14        // Modem configuration
#define DEVIATN         0x15        // Modem deviation setting
#define MCSM2           0x16        // Main Radio Control State Machine configuration
#define MCSM1           0x17        // Main Radio Control State Machine configuration
#define MCSM0           0x18        // Main Radio Control State Machine configuration
#define FOCCFG          0x19        // Frequency Offset Compensation configuration
#define BSCFG           0x1A        // Bit Synchronization configuration
#define AGCCTRL2        0x1B        // AGC control
#define AGCCTRL1        0x1C        // AGC control
#define AGCCTRL0        0x1D        // AGC control
#define WOREVT1         0x1E        // High byte Event 0 timeout
#define WOREVT0         0x1F        // Low byte Event 0 timeout
#define WORCTRL         0x20        // Wake On Radio control
#define FREND1          0x21        // Front end RX configuration
#define FREND0          0x22        // Front end TX configuration
#define FSCAL3          0x23        // Frequency synthesizer calibration
#define FSCAL2          0x24        // Frequency synthesizer calibration
#define FSCAL1          0x25        // Frequency synthesizer calibration
#define FSCAL0          0x26        // Frequency synthesizer calibration
#define RCCTRL1         0x27        // RC oscillator configuration
#define RCCTRL0         0x28        // RC oscillator configuration
#define FSTEST          0x29        // Frequency synthesizer calibration control
#define PTEST           0x2A        // Production test
#define AGCTEST         0x2B        // AGC test
#define TEST2           0x2C        // Various test settings
#define TEST1           0x2D        // Various test settings
#define TEST0           0x2E        // Various test settings

// Status Registers
#define PARTNUM         0x30        // Chip ID
#define VERSION         0x31        // Chip ID
#define FREQEST         0x32        // Frequency Offset Estimate from demodulator
#define LQI             0x33        // Demodulator estimate for Link Quality
#define RSSI            0x34        // Received signal strength indication
#define MARCSTATE       0x35        // Main Radio Control State Machine state
#define WORTIME1        0x36        // High byte of WOR time
#define WORTIME0        0x37        // Low byte of WOR time
#define PKTSTATUS       0x38        // Current GDOx status and packet status
#define VCO_VC_DAC      0x39        // Current setting from PLL calibration module
#define TXBYTES         0x3A        // Underflow and number of bytes
#define RXBYTES         0x3B        // Overflow and number of bytes
#define RCCTRL1_STATUS  0x3C        // Last RC oscillator calibration result
#define RCCTRL0_STATUS  0x3D        // Last RC oscillator calibration result

// PATABLE and FIFO
#define PATABLE         0x3E
#define FIFO            0x3F

// Function Prototypes
void cc1101_init();
void cc1101_reset();
void cc1101_write_reg(uint8_t addr, uint8_t value);
uint8_t cc1101_read_reg(uint8_t addr);
void cc1101_write_burst(uint8_t addr, uint8_t *data, uint8_t len);
void cc1101_read_burst(uint8_t addr, uint8_t *data, uint8_t len);
void cc1101_strobe(uint8_t strobe);
void cc1101_set_freq(uint32_t freq);

#endif // CC1101_DRIVER_H

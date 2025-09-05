#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "cc1101_driver.h"
#include "cc1101_constants.h"

// UART defines for communication with PC
#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

// --- Global State ---
#define PULSE_BUFFER_SIZE 1024
volatile bool capture_mode = false;
volatile bool rssi_mode = false;
volatile bool scan_mode = false; // New flag for frequency scan
char uart_rx_buffer[2048];
uint16_t uart_rx_index = 0;
uint32_t pulse_buffer[PULSE_BUFFER_SIZE];
uint16_t pulse_count = 0;

// --- Function Prototypes ---
void setup_uart();
void handle_uart_command(char* command);
void dump_registers();
void capture_pulses();
void scan_frequencies(char* data);
int16_t convert_rssi(uint8_t rssi_dec);

void setup_uart() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_puts(UART_ID, "\nRP2040 Initialized. Ready for commands.\n");
}

int main()
{
    stdio_init_all();
    printf("RP2040 Booting...\n");
    setup_uart();
    cc1101_init();
    cc1101_configure(); // Initial configuration
    printf("CC1101 Initialized.\n");
    uart_puts(UART_ID, "CC1101 Ready.\n");

    while (true) {
        if (uart_is_readable(UART_ID)) {
            char c = uart_getc(UART_ID);
            if (c == '\n' || c == '\r') {
                if (uart_rx_index > 0) {
                    uart_rx_buffer[uart_rx_index] = '\0';
                    handle_uart_command(uart_rx_buffer);
                    uart_rx_index = 0;
                }
            } else if (uart_rx_index < sizeof(uart_rx_buffer) - 1) {
                uart_rx_buffer[uart_rx_index++] = c;
            }
        }

        if (capture_mode) {
            capture_pulses();
            capture_mode = false;
            uart_puts(UART_ID, "OK: Capture Finished\n");
            printf("Capture finished.\n");
        } else if (rssi_mode) {
            cc1101_strobe(CC1101_SIDLE);
            sleep_us(500);
            cc1101_strobe(CC1101_SRX);
            sleep_us(500);
            uint8_t rssi_raw = cc1101_read_status_reg(CC1101_RSSI);
            int16_t rssi_dbm = convert_rssi(rssi_raw);
            char rssi_msg[32];
            sprintf(rssi_msg, "RSSI_DBM:%d\n", rssi_dbm);
            uart_puts(UART_ID, rssi_msg);
            sleep_ms(100);
        } else if (scan_mode) {
            // Scan logic is handled entirely in the command function
            // This flag just prevents other modes from running
            sleep_ms(100);
        }
        else {
            sleep_ms(10);
        }
    }
    return 0;
}

void handle_uart_command(char* command) {
    printf("Handling command: %s\n", command);
    switch(command[0]) {
        case 'C': // Capture
            if (!capture_mode && !rssi_mode && !scan_mode) {
                printf("Entering Raw Pulse Capture Mode\n");
                uart_puts(UART_ID, "OK: Capturing raw pulses... Press remote.\n");
                capture_mode = true;
            }
            break;
        case 'S': // RSSI Scan
             if (!capture_mode && !rssi_mode && !scan_mode) {
                printf("Entering RSSI Mode\n");
                uart_puts(UART_ID, "OK: RSSI Mode ON\n");
                rssi_mode = true;
            }
            break;
        case 'F': // Frequency Scan
            if (!capture_mode && !rssi_mode && !scan_mode) {
                if (command[1] == ',') {
                    scan_frequencies(command + 2);
                } else {
                    uart_puts(UART_ID, "ERR: Invalid freq scan format.\n");
                }
            }
            break;
        case 'E': // Exit continuous modes
            if (rssi_mode || scan_mode) {
                printf("Exiting Continuous Mode\n");
                uart_puts(UART_ID, "OK: Mode OFF\n");
                cc1101_strobe(CC1101_SIDLE);
                rssi_mode = false;
                scan_mode = false;
            }
            break;
        case 'D': // Dump Registers
            dump_registers();
            break;
        default:
            uart_puts(UART_ID, "ERR: Unknown command\n");
            break;
    }
}

void scan_frequencies(char* data) {
    scan_mode = true;
    uart_puts(UART_ID, "OK: Starting frequency scan. Hold remote button.\n");

    char* start_str = strtok(data, ",");
    char* end_str = strtok(NULL, ",");
    char* step_str = strtok(NULL, ",");

    if (!start_str || !end_str || !step_str) {
        uart_puts(UART_ID, "ERR: Missing scan parameters.\n");
        scan_mode = false;
        return;
    }

    uint32_t start_khz = atoi(start_str);
    uint32_t end_khz = atoi(end_str);
    uint32_t step_khz = atoi(step_str);

    if (step_khz == 0) step_khz = 50;

    for (uint32_t current_khz = start_khz; current_khz <= end_khz; current_khz += step_khz) {
        // Check if a stop command has been received
        if (uart_is_readable(UART_ID) && uart_getc(UART_ID) == 'E') {
            handle_uart_command("E");
            break;
        }

        // Go to IDLE first to ensure a proper recalibration for the new frequency
        cc1101_strobe(CC1101_SIDLE);
        cc1101_set_frequency(current_khz);
        cc1101_strobe(CC1101_SRX);
        sleep_ms(20); // Let receiver settle

        uint8_t rssi_raw = cc1101_read_status_reg(CC1101_RSSI);
        int16_t rssi_dbm = convert_rssi(rssi_raw);

        char scan_msg[40];
        sprintf(scan_msg, "SCAN:%lu,%d\n", current_khz, rssi_dbm);
        uart_puts(UART_ID, scan_msg);
    }

    cc1101_strobe(CC1101_SIDLE);
    uart_puts(UART_ID, "OK: Frequency scan finished.\n");
    scan_mode = false;
}

void capture_pulses() {
    pulse_count = 0;
    cc1101_strobe(CC1101_SRX);
    uint32_t start_time = time_us_32();
    while(!gpio_get(CC1101_PIN_GDO0)) {
        if (time_us_32() - start_time > 2000000) {
            uart_puts(UART_ID, "ERR: Capture timed out.\n");
            cc1101_strobe(CC1101_SIDLE);
            return;
        }
    }
    bool current_state = gpio_get(CC1101_PIN_GDO0);
    uint32_t last_edge_time = time_us_32();
    while(pulse_count < PULSE_BUFFER_SIZE && (time_us_32() - start_time < 5000000)) {
        bool new_state = gpio_get(CC1101_PIN_GDO0);
        if (new_state != current_state) {
            uint32_t now = time_us_32();
            uint32_t duration = now - last_edge_time;
            pulse_buffer[pulse_count++] = duration;
            last_edge_time = now;
            current_state = new_state;
        }
        if (time_us_32() - last_edge_time > 300000) {
            break;
        }
    }
    cc1101_strobe(CC1101_SIDLE);
    if (pulse_count > 0) {
        char temp_buf[20];
        uart_puts(UART_ID, "PULSE:");
        for (int i = 0; i < pulse_count; i++) {
            sprintf(temp_buf, "%lu,", pulse_buffer[i]);
            uart_puts(UART_ID, temp_buf);
        }
        uart_puts(UART_ID, "\n");
    } else {
        uart_puts(UART_ID, "ERR: No pulses captured.\n");
    }
}

void dump_registers() {
    uart_puts(UART_ID, "--- CC1101 Registers ---\n");
    char line[40];
    for (uint8_t i = 0x00; i <= 0x2E; i++) {
        uint8_t value = cc1101_read_reg(i);
        sprintf(line, "Reg 0x%02X: 0x%02X\n", i, value);
        uart_puts(UART_ID, line);
    }
    uart_puts(UART_ID, "------------------------\n");
}

int16_t convert_rssi(uint8_t rssi_raw) {
    int16_t rssi_dbm;
    if (rssi_raw >= 128) {
        rssi_dbm = (int16_t)((int16_t)(rssi_raw - 256) / 2) - 74;
    } else {
        rssi_dbm = (rssi_raw / 2) - 74;
    }
    return rssi_dbm;
}

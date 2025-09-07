#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "cc1101_driver.h"

#include "signal_capture.pio.h"
#include "signal_transmit.pio.h"

#define CAPTURE_SIZE 200
#define UART_BUFFER_SIZE 2048

// Helper function to read a line from UART
int read_line(char *buffer, int max_len) {
    int i = 0;
    while (i < max_len - 1) {
        int c = getchar_timeout_us(1000 * 1000); // 1 second timeout
        if (c == PICO_ERROR_TIMEOUT) return -1;
        if (c == '\n' || c == '\r') break;
        buffer[i++] = (char)c;
    }
    buffer[i] = '\0';
    return i;
}

int main()
{
    stdio_init_all();
    sleep_ms(2000); // Wait for serial monitor to connect

    cc1101_init();

    printf("RP2040 CC1101 Signal Cloner\n");
    printf("Send 'c' to start capture.\n");
    printf("Send 't' to start transmit, followed by comma-separated pulse durations in us on a new line.\n");

    // The main loop
    while (true) {
        int c = getchar_timeout_us(10000);

        if (c == 'c') {
            printf("Starting capture for %d pulses...\n", CAPTURE_SIZE);

            PIO pio = pio0;
            uint sm = pio_claim_unused_sm(pio, true);
            uint offset = pio_add_program(pio, &signal_capture_program);

            float clk_div = 1.0f;
            signal_capture_program_init(pio, sm, offset, PIN_GDO0, clk_div);

            cc1101_strobe(SRX);
            sleep_ms(1);

            uint32_t captured_data[CAPTURE_SIZE];
            for (int i = 0; i < CAPTURE_SIZE; ++i) {
                captured_data[i] = pio_sm_get_blocking(pio, sm);
            }

            pio_sm_set_enabled(pio, sm, false);
            pio_remove_program(pio, &signal_capture_program, offset);
            pio_sm_unclaim(pio, sm);
            cc1101_strobe(SIDLE);

            printf("Capture finished. Data (in microseconds):\n");
            float sys_clk_mhz = (float)clock_get_hz(clk_sys) / 1000000.f;
            for (int i = 0; i < CAPTURE_SIZE; ++i) {
                uint32_t raw_value = captured_data[i];
                float duration_us = (float)(0xFFFFFFFF - raw_value) * 2.f / sys_clk_mhz;
                printf("%.2f%s", duration_us, (i == CAPTURE_SIZE - 1) ? "" : ",");
            }
            printf("\n");

        } else if (c == 't') {
            printf("Waiting for transmit data...\n");

            static char uart_buf[UART_BUFFER_SIZE];
            int len = read_line(uart_buf, sizeof(uart_buf));

            if (len <= 0) {
                printf("Failed to read transmit data.\n");
                continue;
            }
            printf("Received %d bytes. Parsing and preparing for transmission...\n", len);

            PIO pio = pio1;
            uint sm = pio_claim_unused_sm(pio, true);
            uint offset = pio_add_program(pio, &signal_transmit_program);

            float sys_clk_mhz = (float)clock_get_hz(clk_sys) / 1000000.f;
            // The transmit PIO loop takes 33 cycles.
            float cycles_per_us = sys_clk_mhz / 33.0f;

            char *token = strtok(uart_buf, ",");
            int pulse_count = 0;
            while(token != NULL) {
                float duration_us = strtof(token, NULL);
                if (duration_us > 0) {
                    uint32_t pio_cycles = (uint32_t)(duration_us * cycles_per_us);
                    pio_sm_put_blocking(pio, sm, pio_cycles);
                    pulse_count++;
                }
                token = strtok(NULL, ",");
            }

            printf("Transmitting %d pulses...\n", pulse_count);

            signal_transmit_program_init(pio, sm, offset, PIN_GDO2, 1.0f);
            cc1101_strobe(STX);

            // Wait for PIO to finish (TX FIFO is empty)
            while(!pio_sm_is_tx_fifo_empty(pio, sm)) {
                sleep_ms(1);
            }
            sleep_ms(10); // Allow last pulse to finish

            pio_sm_set_enabled(pio, sm, false);
            pio_remove_program(pio, &signal_transmit_program, offset);
            pio_sm_unclaim(pio, sm);
            cc1101_strobe(SIDLE);

            printf("Transmit finished.\n");
        }
    }
}

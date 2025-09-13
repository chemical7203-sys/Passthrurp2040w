#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "tusb.h"
#include "bsp/board.h"
#include "class/hid/hid_device.h"

#if CFG_TUD_HID_SONY
#include "ds4_report.h"
#endif

// GPIO Pin for the 433MHz RF Transmitter's DATA line
#define RF_TX_PIN 15

// Struct to hold the received v2 controller data from UART
typedef struct __attribute__((packed)) {
    uint16_t buttons;
    int8_t   lx, ly;
    uint8_t  l2, r2;
    int8_t   rx, ry;
    uint8_t  dpad;
    int16_t  accel_x, accel_y, accel_z;
    int16_t  gyro_x, gyro_y, gyro_z;
} gamepad_data_v2_t;

static gamepad_data_v2_t gamepad_data;

// --- RF 433MHz rc-switch implementation ---
// Timings for the rc-switch protocol (Type 1)
#define RC_PULSE_LENGTH 190  // in microseconds
#define RC_REPEATS      10   // Number of times to repeat the transmission

void transmit(int high_pulses, int low_pulses) {
    gpio_put(RF_TX_PIN, 1);
    sleep_us(RC_PULSE_LENGTH * high_pulses);
    gpio_put(RF_TX_PIN, 0);
    sleep_us(RC_PULSE_LENGTH * low_pulses);
}

void send_rc_code(unsigned long code) {
    for (int n = 0; n < RC_REPEATS; n++) {
        // Send the 24-bit code
        for (int i = 23; i >= 0; i--) {
            if ((code >> i) & 1) {
                // '1' bit: 3 high, 1 low pulse lengths
                transmit(3, 1);
            } else {
                // '0' bit: 1 high, 3 low pulse lengths
                transmit(1, 3);
            }
        }
        // Send Sync pulse (1 high, 31 low)
        transmit(1, 31);
    }
}

// --- UART and CRC8 ---
#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

void setup_uart() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

void debug_puts(const char *s) { uart_puts(UART_ID, s); }

static uint8_t crc8_table[256];
static void build_crc8_table(uint8_t poly) {
    for (uint16_t i = 0; i < 256; i++) {
        uint8_t c = i;
        for (uint8_t j = 0; j < 8; j++) {
            c = ((c & 0x80) == 0) ? (c << 1) : ((c << 1) ^ poly);
        }
        crc8_table[i] = c;
    }
}

static uint8_t crc8(uint8_t *data, int len, uint8_t init) {
    uint8_t crc = init;
    while (len-- > 0) {
        crc = crc8_table[(crc ^ *data++)];
    }
    return crc;
}

// Refactored UART processing to handle multiple packet types
void process_uart() {
    static uint8_t packet_buffer[32]; // Increased size for safety
    static uint8_t packet_idx = 0;
    static uint8_t expected_len = 0;

    while (uart_is_readable(UART_ID)) {
        uint8_t ch = uart_getc(UART_ID);

        if (packet_idx == 0) { // Waiting for a header
            if (ch == 0xA6) { // Gamepad Data Packet
                packet_buffer[0] = ch;
                expected_len = 23;
                packet_idx = 1;
            } else if (ch == 0xA7) { // RF Code Packet
                packet_buffer[0] = ch;
                expected_len = 6; // 1 header + 4 code bytes + 1 crc
                packet_idx = 1;
            }
        } else { // Receiving a packet
            packet_buffer[packet_idx++] = ch;

            if (packet_idx >= expected_len) {
                // Full packet received, now process it
                uint8_t calculated_crc = crc8(packet_buffer, expected_len - 1, 0xFF);
                uint8_t received_crc = packet_buffer[expected_len - 1];

                if (calculated_crc == received_crc) {
                    if (packet_buffer[0] == 0xA6) {
                        memcpy(&gamepad_data, &packet_buffer[1], sizeof(gamepad_data));
                    } else if (packet_buffer[0] == 0xA7) {
                        unsigned long rf_code;
                        memcpy(&rf_code, &packet_buffer[1], sizeof(rf_code));
                        send_rc_code(rf_code);
                    }
                }
                // Reset for next packet
                packet_idx = 0;
                expected_len = 0;
            }
        }
    }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  (void) instance; (void) buffer; (void) reqlen; (void) report_id; (void) report_type;
  return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) bufsize;
}

void hid_task(void) {
  const uint32_t interval_ms = 16;
  static uint32_t start_ms = 0;
  if ( board_millis() - start_ms < interval_ms) return;
  start_ms += interval_ms;

  if ( tud_suspended() ) tud_remote_wakeup();

  #if CFG_TUD_HID_SONY
    if ( tud_hid_ready() ) {
      hid_ds4_report_t report = {0};
      report.report_id = 0x01;
      report.left_stick_x = gamepad_data.lx + 128;
      report.left_stick_y = gamepad_data.ly + 128;
      report.right_stick_x = gamepad_data.rx + 128;
      report.right_stick_y = gamepad_data.ry + 128;
      report.l2_trigger = gamepad_data.l2;
      report.r2_trigger = gamepad_data.r2;

      static const uint8_t dpad_map[16] = { 15, 0, 4, 15, 6, 7, 5, 15, 2, 1, 3, 15, 15, 15, 15, 15 };
      report.dpad = dpad_map[gamepad_data.dpad & 0x0F];

      report.cross = (gamepad_data.buttons >> 0) & 1;
      report.circle = (gamepad_data.buttons >> 1) & 1;
      report.square = (gamepad_data.buttons >> 2) & 1;
      report.triangle = (gamepad_data.buttons >> 3) & 1;
      report.l1 = (gamepad_data.buttons >> 4) & 1;
      report.r1 = (gamepad_data.buttons >> 5) & 1;
      report.l2 = (gamepad_data.buttons >> 6) & 1;
      report.r2 = (gamepad_data.buttons >> 7) & 1;
      report.share = (gamepad_data.buttons >> 8) & 1;
      report.options = (gamepad_data.buttons >> 9) & 1;
      report.l3 = (gamepad_data.buttons >> 10) & 1;
      report.r3 = (gamepad_data.buttons >> 11) & 1;
      report.ps = (gamepad_data.buttons >> 12) & 1;
      report.tpad_click = (gamepad_data.buttons >> 13) & 1;

      report.accel_x = gamepad_data.accel_x;
      report.accel_y = gamepad_data.accel_y;
      report.accel_z = gamepad_data.accel_z;
      report.gyro_x = gamepad_data.gyro_x;
      report.gyro_y = gamepad_data.gyro_y;
      report.gyro_z = gamepad_data.gyro_z;

      static uint8_t ds4_report_counter = 0;
      report.report_counter = ds4_report_counter++;
      tud_hid_report(0, &report, sizeof(report));
    }
  #endif
}

int main() {
    board_init();
    setup_uart();
    build_crc8_table(0x07);
    tusb_init();

    // Setup RF Transmitter Pin
    gpio_init(RF_TX_PIN);
    gpio_set_dir(RF_TX_PIN, GPIO_OUT);
    gpio_put(RF_TX_PIN, 0); // Start with pin low

    while (true) {
        tud_task();
        hid_task();
        process_uart();
    }
    return 0;
}

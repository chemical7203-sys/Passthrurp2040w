#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h> // For abs()
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/uart.h"
#include "tusb.h"
#include "bsp/board.h"
#include "class/hid/hid_device.h"

#if CFG_TUD_HID_SONY
#include "ds4_report.h"
#endif

// --- Pin Definitions ---
#define RF_TX_PIN 15 // RF Transmitter
#define RF_RX_PIN 16 // RF Receiver

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

// --- RF 433MHz rc-switch: Transmit Implementation ---
typedef struct { uint8_t high; uint8_t low; } HighLow;
typedef struct { uint16_t pulseLength; HighLow syncFactor; HighLow zero; HighLow one; } Protocol_t;

const Protocol_t tx_protocol = { 350, { 1, 31 }, { 1, 3 }, { 3, 1 } };
const int nRepeatTransmit = 10;

void transmit(HighLow pulses) {
    gpio_put(RF_TX_PIN, 1);
    sleep_us(tx_protocol.pulseLength * pulses.high);
    gpio_put(RF_TX_PIN, 0);
    sleep_us(tx_protocol.pulseLength * pulses.low);
}

void send_rc_code(unsigned long code, unsigned int length) {
    for (int n = 0; n < nRepeatTransmit; n++) {
        for (int i = length - 1; i >= 0; i--) {
            if ((code >> i) & 1) transmit(tx_protocol.one);
            else transmit(tx_protocol.zero);
        }
        transmit(tx_protocol.syncFactor);
    }
}

// --- RF 433MHz rc-switch: Receive Implementation ---
#define RCSWITCH_MAX_CHANGES 67
const Protocol_t rx_protocols[] = {
  { 350, {  1, 31 }, {  1,  3 }, {  3,  1 } },    // protocol 1
  { 650, {  1, 10 }, {  1,  2 }, {  2,  1 } },    // protocol 2
  { 100, { 30, 71 }, {  4, 11 }, {  9,  6 } },    // protocol 3
  { 380, {  1,  6 }, {  1,  3 }, {  3,  1 } },    // protocol 4
  { 500, {  6, 14 }, {  1,  2 }, {  2,  1 } }     // protocol 5
};
const int num_rx_protocols = sizeof(rx_protocols) / sizeof(rx_protocols[0]);

volatile unsigned int timings[RCSWITCH_MAX_CHANGES];
volatile unsigned long nReceivedValue = 0;
volatile unsigned int nReceivedBitlength = 0;
volatile unsigned int nReceivedDelay = 0;
volatile unsigned int nReceivedProtocol = 0;
volatile bool nAvailable = false;
const int nReceiveTolerance = 60;
const unsigned int nSeparationLimit = 4300;

void rf_receive_reset_available() { nAvailable = false; }

bool rf_receive_decode(unsigned int p_index, unsigned int change_count) {
    Protocol_t p = rx_protocols[p_index];
    unsigned long code = 0;
    unsigned int sync_length_in_pulses = (p.syncFactor.low > p.syncFactor.high) ? p.syncFactor.low : p.syncFactor.high;
    unsigned int delay = timings[0] / sync_length_in_pulses;
    unsigned int delay_tolerance = delay * nReceiveTolerance / 100;

    for (unsigned int i = 1; i < change_count - 1; i += 2) {
        code <<= 1;
        if (abs(timings[i] - delay * p.zero.high) < delay_tolerance && abs(timings[i+1] - delay * p.zero.low) < delay_tolerance) {
            // zero
        } else if (abs(timings[i] - delay * p.one.high) < delay_tolerance && abs(timings[i+1] - delay * p.one.low) < delay_tolerance) {
            code |= 1; // one
        } else {
            return false; // Failed
        }
    }

    if (change_count > 7) {
        nReceivedValue = code;
        nReceivedBitlength = (change_count - 1) / 2;
        nReceivedDelay = delay;
        nReceivedProtocol = p_index + 1;
        nAvailable = true;
        return true;
    }
    return false;
}

void gpio_callback(uint gpio, uint32_t events) {
    static unsigned int change_count = 0;
    static unsigned long last_time = 0;
    static unsigned int repeat_count = 0;

    const long time = time_us_32();
    const unsigned int duration = time - last_time;

    if (duration > nSeparationLimit) {
        if (abs(duration - timings[0]) < 200) {
            repeat_count++;
            if (repeat_count == 2) {
                for (unsigned int i = 0; i < num_rx_protocols; i++) {
                    if (rf_receive_decode(i, change_count)) {
                        break;
                    }
                }
                repeat_count = 0;
            }
        }
        change_count = 0;
    }

    if (change_count >= RCSWITCH_MAX_CHANGES) {
        change_count = 0;
        repeat_count = 0;
    }

    timings[change_count++] = duration;
    last_time = time;
}

// --- UART and CRC8 ---
#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5
void setup_uart() { uart_init(UART_ID, BAUD_RATE); gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART); gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART); }
void debug_puts(const char *s) { uart_puts(UART_ID, s); }
static uint8_t crc8_table[256];
static void build_crc8_table(uint8_t poly) { for (uint16_t i = 0; i < 256; i++) { uint8_t c = i; for (uint8_t j = 0; j < 8; j++) { c = ((c & 0x80) == 0) ? (c << 1) : ((c << 1) ^ poly); } crc8_table[i] = c; } }
static uint8_t crc8(uint8_t *data, int len, uint8_t init) { uint8_t crc = init; while (len-- > 0) { crc = crc8_table[(crc ^ *data++)]; } return crc; }

void process_uart() {
    static uint8_t packet_buffer[32], packet_idx = 0, expected_len = 0;
    while (uart_is_readable(UART_ID)) {
        uint8_t ch = uart_getc(UART_ID);
        if (packet_idx == 0) {
            if (ch == 0xA6) expected_len = 23; else if (ch == 0xA7) expected_len = 6; else continue;
            packet_buffer[0] = ch; packet_idx = 1;
        } else {
            packet_buffer[packet_idx++] = ch;
            if (packet_idx >= expected_len) {
                if (crc8(packet_buffer, expected_len-1, 0xFF) == packet_buffer[expected_len-1]) {
                    if (packet_buffer[0] == 0xA6) memcpy(&gamepad_data, &packet_buffer[1], sizeof(gamepad_data));
                    else if (packet_buffer[0] == 0xA7) { unsigned long rf_code; memcpy(&rf_code, &packet_buffer[1], sizeof(rf_code)); send_rc_code(rf_code, 24); }
                }
                packet_idx = 0;
            }
        }
    }
}

// Task to check for received RF data and send it over UART
void rf_receive_task() {
    if (nAvailable) {
        char buffer[128];
        sprintf(buffer, "RF_RX: value=%lu length=%u delay=%uus protocol=%u\r\n", nReceivedValue, nReceivedBitlength, nReceivedDelay, nReceivedProtocol);
        debug_puts(buffer);
        rf_receive_reset_available();
    }
}

uint16_t tud_hid_get_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t* b, uint16_t l) { return 0; }
void tud_hid_set_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t const* b, uint16_t l) {}

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
      report.left_stick_x = gamepad_data.lx + 128; report.left_stick_y = gamepad_data.ly + 128;
      report.right_stick_x = gamepad_data.rx + 128; report.right_stick_y = gamepad_data.ry + 128;
      report.l2_trigger = gamepad_data.l2; report.r2_trigger = gamepad_data.r2;
      static const uint8_t dpad_map[16] = { 15, 0, 4, 15, 6, 7, 5, 15, 2, 1, 3, 15, 15, 15, 15, 15 };
      report.dpad = dpad_map[gamepad_data.dpad & 0x0F];
      report.cross = (gamepad_data.buttons >> 0) & 1; report.circle = (gamepad_data.buttons >> 1) & 1;
      report.square = (gamepad_data.buttons >> 2) & 1; report.triangle = (gamepad_data.buttons >> 3) & 1;
      report.l1 = (gamepad_data.buttons >> 4) & 1; report.r1 = (gamepad_data.buttons >> 5) & 1;
      report.l2 = (gamepad_data.buttons >> 6) & 1; report.r2 = (gamepad_data.buttons >> 7) & 1;
      report.share = (gamepad_data.buttons >> 8) & 1; report.options = (gamepad_data.buttons >> 9) & 1;
      report.l3 = (gamepad_data.buttons >> 10) & 1; report.r3 = (gamepad_data.buttons >> 11) & 1;
      report.ps = (gamepad_data.buttons >> 12) & 1; report.tpad_click = (gamepad_data.buttons >> 13) & 1;
      report.accel_x = gamepad_data.accel_x; report.accel_y = gamepad_data.accel_y; report.accel_z = gamepad_data.accel_z;
      report.gyro_x = gamepad_data.gyro_x; report.gyro_y = gamepad_data.gyro_y; report.gyro_z = gamepad_data.gyro_z;
      static uint8_t ds4_report_counter = 0; report.report_counter = ds4_report_counter++;
      tud_hid_report(0, &report, sizeof(report));
    }
  #endif
}

int main() {
    board_init();
    setup_uart();
    build_crc8_table(0x07);
    tusb_init();

    // Setup RF Pins
    gpio_init(RF_TX_PIN);
    gpio_set_dir(RF_TX_PIN, GPIO_OUT);
    gpio_put(RF_TX_PIN, 0);

    gpio_init(RF_RX_PIN);
    gpio_set_dir(RF_RX_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(RF_RX_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    while (true) {
        tud_task();
        hid_task();
        process_uart();
        rf_receive_task();
    }
    return 0;
}

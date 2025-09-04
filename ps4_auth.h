/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 *
 * This file is a C-language port of the PS4 authentication logic
 * from the GP2040-CE project.
 */

#ifndef _PS4_AUTH_H_
#define _PS4_AUTH_H_

#include "mbedtls/rsa.h"
#include <stdbool.h>
#include <stdint.h>

// Define the states for PS4 authentication
typedef enum {
    auth_idle_state,
    send_auth_console_to_dongle,
    send_auth_dongle_to_console,
} GPAuthState;

// PS4 Auth Data in a single struct
typedef struct {
    struct mbedtls_rsa_context rsa_context;
    uint8_t ps4_auth_buffer[1064];
    bool valid_rsa;
    GPAuthState passthrough_state;
    uint8_t nonce_id;
} PS4AuthData;


// Function prototypes for the PS4 authentication logic
void ps4_auth_initialize(PS4AuthData* auth_data);
void ps4_auth_process(PS4AuthData* auth_data);
void ps4_auth_reset(PS4AuthData* auth_data);

#endif // _PS4_AUTH_H_

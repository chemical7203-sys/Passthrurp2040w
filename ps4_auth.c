/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 *
 * This file is a C-language port of the PS4 authentication logic
 * from the GP2040-CE project.
 */

#include "ps4_auth.h"
#include <string.h>
#include <stdlib.h> // For rand()

#include "mbedtls/error.h"
#include "mbedtls/rsa.h"
#include "mbedtls/sha256.h"

// ************************************************************************************
// ** IMPORTANT: PLACEHOLDER KEYS AND SERIAL/SIGNATURE                             **
// ** The following arrays are placeholders. For the authentication to succeed,    **
// ** you MUST replace these with your own valid PS4 RSA private key components,   **
// ** serial number, and signature. The firmware will compile with these           **
// ** placeholders, but it will not authenticate with a PS4/PS5 console.           **
// ************************************************************************************

// 256-byte RSA private key exponent P
static const uint8_t rsa_key_p[128] = {
    0x00 // ... 128 bytes of placeholder data
};

// 256-byte RSA private key exponent Q
static const uint8_t rsa_key_q[128] = {
    0x00 // ... 128 bytes of placeholder data
};

// 256-byte RSA private key modulus N
static const uint8_t rsa_key_n[256] = {
    0x00 // ... 256 bytes of placeholder data
};

// 256-byte RSA public key exponent E
static const uint8_t rsa_key_e[3] = {
    0x01, 0x00, 0x01 // Standard exponent
};

// 16-byte device serial number
static const uint8_t ps4_serial[16] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
};

// 256-byte device signature
static const uint8_t ps4_signature[256] = {
    0x00 // ... 256 bytes of placeholder data
};


// A simple random number generator for mbedtls
static int rng_callback(void* p_rng, unsigned char* p, size_t len) {
    (void) p_rng;
    for (size_t i = 0; i < len; i++) {
        p[i] = rand();
    }
    return 0;
}

void ps4_auth_initialize(PS4AuthData* auth_data) {
    auth_data->valid_rsa = false;
    auth_data->passthrough_state = auth_idle_state;
    auth_data->nonce_id = 0;
    memset(auth_data->ps4_auth_buffer, 0, sizeof(auth_data->ps4_auth_buffer));

    mbedtls_rsa_init(&auth_data->rsa_context, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);

    mbedtls_mpi N, P, Q, E;
    mbedtls_mpi_init(&N);
    mbedtls_mpi_init(&P);
    mbedtls_mpi_init(&Q);
    mbedtls_mpi_init(&E);

    // Import the key components into mbedtls MPI format
    mbedtls_mpi_read_binary(&N, rsa_key_n, sizeof(rsa_key_n));
    mbedtls_mpi_read_binary(&P, rsa_key_p, sizeof(rsa_key_p));
    mbedtls_mpi_read_binary(&Q, rsa_key_q, sizeof(rsa_key_q));
    mbedtls_mpi_read_binary(&E, rsa_key_e, sizeof(rsa_key_e));

    // Populate the RSA context
    if (mbedtls_rsa_import(&auth_data->rsa_context, &N, &P, &Q, NULL, &E) == 0 &&
        mbedtls_rsa_complete(&auth_data->rsa_context) == 0) {
        auth_data->valid_rsa = true;
    }

    mbedtls_mpi_free(&N);
    mbedtls_mpi_free(&P);
    mbedtls_mpi_free(&Q);
    mbedtls_mpi_free(&E);

    // Seed the random number generator
    srand(0);
}

void ps4_auth_process(PS4AuthData* auth_data) {
    if (!auth_data->valid_rsa) {
        return;
    }

    if (auth_data->passthrough_state == send_auth_console_to_dongle) {
        uint8_t hashed_nonce[32];
        int sign_result;

        // Hash the 256-byte nonce received from the console
        if (mbedtls_sha256_ret(auth_data->ps4_auth_buffer, 256, hashed_nonce, 0) != 0) {
            auth_data->passthrough_state = auth_idle_state;
            return;
        }

        // Sign the hash with our private key
        sign_result = mbedtls_rsa_rsassa_pss_sign(
            &auth_data->rsa_context,
            rng_callback,
            NULL,
            MBEDTLS_RSA_PRIVATE,
            MBEDTLS_MD_SHA256,
            32,
            hashed_nonce,
            auth_data->ps4_auth_buffer // Write the 256-byte signature back to the start of the buffer
        );

        if (sign_result != 0) {
            auth_data->passthrough_state = auth_idle_state;
            return;
        }

        // The signed nonce is now in ps4_auth_buffer[0-255].
        // Now, we append the rest of the required data to the buffer.
        size_t offset = 256;

        // 16-byte serial
        memcpy(&auth_data->ps4_auth_buffer[offset], ps4_serial, sizeof(ps4_serial));
        offset += sizeof(ps4_serial);

        // 256-byte RSA_N, 256-byte RSA_E
        mbedtls_rsa_export_raw(
            &auth_data->rsa_context,
            &auth_data->ps4_auth_buffer[offset], 256,         // N
            NULL, 0,                                          // P
            NULL, 0,                                          // Q
            NULL, 0,                                          // D
            &auth_data->ps4_auth_buffer[offset + 256], 256  // E
        );
        offset += 512;

        // 256-byte Signature
        memcpy(&auth_data->ps4_auth_buffer[offset], ps4_signature, sizeof(ps4_signature));
        offset += sizeof(ps4_signature);

        // 24-byte zero padding
        memset(&auth_data->ps4_auth_buffer[offset], 0, 24);

        // Mark as ready to send back to console
        auth_data->passthrough_state = send_auth_dongle_to_console;
    }
}

void ps4_auth_reset(PS4AuthData* auth_data) {
    auth_data->passthrough_state = auth_idle_state;
}

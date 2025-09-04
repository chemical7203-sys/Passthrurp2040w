#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

// Platform settings
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES

// Required for PS4 Auth
#define MBEDTLS_RSA_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_PKCS1_V21
#define MBEDTLS_MD_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_CTR_DRBG_C

// Other settings to keep the build minimal
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_OID_C
#define MBEDTLS_PKCS1_V15

#endif /* MBEDTLS_CONFIG_H */

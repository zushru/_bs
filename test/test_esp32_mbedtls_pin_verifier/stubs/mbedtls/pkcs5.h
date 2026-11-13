#pragma once

#include <stddef.h>
#include <stdint.h>
#include "md.h"

#ifdef __cplusplus
extern "C" {
#endif

int mbedtls_pkcs5_pbkdf2_hmac(
    mbedtls_md_context_t* ctx,
    const unsigned char* password, size_t password_length,
    const unsigned char* salt, size_t salt_length,
    unsigned int iterations, uint32_t key_length,
    unsigned char* output);

#ifdef __cplusplus
}
#endif

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  MBEDTLS_MD_NONE = 0,
  MBEDTLS_MD_SHA256 = 6
} mbedtls_md_type_t;

typedef struct mbedtls_md_info_t {
  mbedtls_md_type_t type;
} mbedtls_md_info_t;

typedef struct mbedtls_md_context_t {
  const mbedtls_md_info_t* md_info;
  int hmac;
} mbedtls_md_context_t;

const mbedtls_md_info_t* mbedtls_md_info_from_type(mbedtls_md_type_t type);
void mbedtls_md_init(mbedtls_md_context_t* ctx);
int mbedtls_md_setup(mbedtls_md_context_t* ctx,
                     const mbedtls_md_info_t* info, int hmac);
void mbedtls_md_free(mbedtls_md_context_t* ctx);

#ifdef __cplusplus
}
#endif

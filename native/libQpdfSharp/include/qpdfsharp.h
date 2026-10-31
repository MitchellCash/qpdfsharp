/*
 * qpdfsharp.h - the C ABI exported by libQpdfSharp.
 *
 * This is the only interface the managed QpdfSharp library binds to. qpdf itself is linked
 * statically and none of its symbols are exported. Bump QPDFSHARP_ABI_VERSION whenever this
 * header changes incompatibly.
 */

#ifndef QPDFSHARP_H
#define QPDFSHARP_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(QPDFSHARP_BUILDING)
#    define QPDFSHARP_API __declspec(dllexport)
#  else
#    define QPDFSHARP_API __declspec(dllimport)
#  endif
#else
#  define QPDFSHARP_API __attribute__((visibility("default")))
#endif

#define QPDFSHARP_ABI_VERSION 1

#ifdef __cplusplus
extern "C" {
#endif

/* The version of the statically linked qpdf, for example "12.4.2". Static storage; never NULL. */
QPDFSHARP_API const char* qpdfsharp_get_qpdf_version(void);

/* Returns QPDFSHARP_ABI_VERSION as compiled into the library. */
QPDFSHARP_API int32_t qpdfsharp_get_abi_version(void);

typedef enum qpdfsharp_status {
    QPDFSHARP_STATUS_OK = 0,
    QPDFSHARP_STATUS_INVALID_ARGUMENT = 1,
    QPDFSHARP_STATUS_PASSWORD_REQUIRED = 2,
    QPDFSHARP_STATUS_INVALID_PDF = 3,
    QPDFSHARP_STATUS_INTERNAL_ERROR = 4
} qpdfsharp_status;

/* Actions allowed to anyone who opens the file without the owner password. */
#define QPDFSHARP_ALLOW_PRINT_LOW_RESOLUTION  (1u << 0)
#define QPDFSHARP_ALLOW_PRINT_HIGH_RESOLUTION (1u << 1) /* implies low resolution */
#define QPDFSHARP_ALLOW_EXTRACT_CONTENT       (1u << 2)
#define QPDFSHARP_ALLOW_ASSEMBLE_DOCUMENT     (1u << 3)
#define QPDFSHARP_ALLOW_ANNOTATE_AND_FORM     (1u << 4)
#define QPDFSHARP_ALLOW_FILL_FORMS            (1u << 5)
#define QPDFSHARP_ALLOW_MODIFY_CONTENT        (1u << 6)
#define QPDFSHARP_ALLOW_ALL                   (0x7Fu)

typedef struct qpdfsharp_encryption_info {
    int32_t is_encrypted;           /* 0 or 1 */
    int32_t revision;               /* R value; 0 when not encrypted */
    int32_t owner_password_matched; /* 0 or 1 */
    int32_t user_password_matched;  /* 0 or 1 */
    uint32_t permissions;           /* QPDFSHARP_ALLOW_* bits */
} qpdfsharp_encryption_info;

/* Encrypts a PDF with AES-256 (R6), an empty user password and the given owner password.

   On OK, *output is a buffer the caller must release with qpdfsharp_free_buffer.
   On any other status, *output is NULL and *output_length is 0.
   error_message (may be NULL when capacity is 0) always receives a NUL-terminated,
   possibly truncated, UTF-8 message; empty on OK. */
QPDFSHARP_API qpdfsharp_status qpdfsharp_protect_with_owner_password(
    const uint8_t* input, size_t input_length,
    const char* owner_password,          /* NUL-terminated UTF-8, 1..127 bytes */
    uint32_t allowed_permissions,        /* only QPDFSHARP_ALLOW_* bits */
    uint8_t** output, size_t* output_length,
    char* error_message, size_t error_message_capacity);

/* Opens a PDF with the given password and reports how it is encrypted. On any status other than
   OK, *info is zeroed. error_message behaves as for qpdfsharp_protect_with_owner_password. */
QPDFSHARP_API qpdfsharp_status qpdfsharp_inspect_encryption(
    const uint8_t* input, size_t input_length,
    const char* password,                /* NUL-terminated UTF-8; "" for none */
    qpdfsharp_encryption_info* info,
    char* error_message, size_t error_message_capacity);

/* Releases a buffer returned by qpdfsharp_protect_with_owner_password. NULL is a no-op. */
QPDFSHARP_API void qpdfsharp_free_buffer(uint8_t* buffer);

#ifdef __cplusplus
}
#endif

#endif /* QPDFSHARP_H */

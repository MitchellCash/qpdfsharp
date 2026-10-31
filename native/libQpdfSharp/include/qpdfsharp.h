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

#ifdef __cplusplus
}
#endif

#endif /* QPDFSHARP_H */

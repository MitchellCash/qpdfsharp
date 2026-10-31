/* Smoke test: the library loads and reports the expected qpdf and ABI versions. */

#include <stdio.h>
#include <string.h>

#include "qpdfsharp.h"

#define EXPECTED_QPDF_VERSION "12.4.2"

int main(void)
{
    int failures = 0;

    const char* version = qpdfsharp_get_qpdf_version();
    if (version == NULL) {
        fprintf(stderr, "FAIL: qpdfsharp_get_qpdf_version() returned NULL\n");
        failures++;
    } else if (strncmp(version, EXPECTED_QPDF_VERSION, strlen(EXPECTED_QPDF_VERSION)) != 0) {
        fprintf(stderr, "FAIL: qpdf version is \"%s\", expected \"%s\"\n", version, EXPECTED_QPDF_VERSION);
        failures++;
    }

    int32_t abi_version = qpdfsharp_get_abi_version();
    if (abi_version != QPDFSHARP_ABI_VERSION) {
        fprintf(stderr, "FAIL: ABI version is %d, expected %d\n", (int)abi_version, QPDFSHARP_ABI_VERSION);
        failures++;
    }

    if (failures != 0) {
        return 1;
    }
    printf("smoke: qpdf %s, ABI %d\n", version, (int)abi_version);
    return 0;
}

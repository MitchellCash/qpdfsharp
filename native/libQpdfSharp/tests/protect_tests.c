/* Native tests for qpdfsharp_protect_with_owner_password and qpdfsharp_inspect_encryption. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qpdfsharp.h"

#ifndef QPDFSHARP_TEST_CONTENT_DIR
#  error "QPDFSHARP_TEST_CONTENT_DIR must point at tests/Content/pdfs"
#endif

#define OWNER_PASSWORD "owner-secret"
#define ERROR_MESSAGE_CAPACITY 1024

static int failures = 0;

#define CHECK(test_name, condition)                                                        \
    do {                                                                                   \
        if (!(condition)) {                                                                \
            fprintf(stderr, "FAIL: %s: %s (line %d)\n", test_name, #condition, __LINE__);  \
            failures++;                                                                    \
        }                                                                                  \
    } while (0)

typedef struct pdf_bytes {
    uint8_t* data;
    size_t length;
} pdf_bytes;

/* Reads tests/Content/pdfs/<name>; exits the test run if the asset is missing. */
static pdf_bytes read_test_pdf(const char* name)
{
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", QPDFSHARP_TEST_CONTENT_DIR, name);

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "error: cannot open test asset %s\n", path);
        exit(2);
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    pdf_bytes pdf;
    pdf.length = (size_t)length;
    pdf.data = (uint8_t*)malloc(pdf.length);
    if (pdf.data == NULL || fread(pdf.data, 1, pdf.length, file) != pdf.length) {
        fprintf(stderr, "error: cannot read test asset %s\n", path);
        exit(2);
    }
    fclose(file);
    return pdf;
}

/* Protects minimal.pdf with OWNER_PASSWORD; the caller frees the result with qpdfsharp_free_buffer. */
static qpdfsharp_status protect_minimal(uint32_t allowed_permissions, pdf_bytes* protected_pdf)
{
    pdf_bytes input = read_test_pdf("minimal.pdf");
    char error_message[ERROR_MESSAGE_CAPACITY];
    qpdfsharp_status status = qpdfsharp_protect_with_owner_password(
        input.data, input.length, OWNER_PASSWORD, allowed_permissions,
        &protected_pdf->data, &protected_pdf->length, error_message, sizeof error_message);
    if (status != QPDFSHARP_STATUS_OK) {
        fprintf(stderr, "protect_minimal: status %d: %s\n", (int)status, error_message);
    }
    free(input.data);
    return status;
}

static qpdfsharp_status inspect(pdf_bytes pdf, const char* password, qpdfsharp_encryption_info* info)
{
    char error_message[ERROR_MESSAGE_CAPACITY];
    qpdfsharp_status status = qpdfsharp_inspect_encryption(
        pdf.data, pdf.length, password, info, error_message, sizeof error_message);
    if (status != QPDFSHARP_STATUS_OK) {
        fprintf(stderr, "inspect: status %d: %s\n", (int)status, error_message);
    }
    return status;
}

static void protect_minimal_ok(void)
{
    const char* name = "protect_minimal_ok";
    pdf_bytes output = { NULL, 0 };

    CHECK(name, protect_minimal(0, &output) == QPDFSHARP_STATUS_OK);
    CHECK(name, output.data != NULL);
    CHECK(name, output.length > 0);
    CHECK(name, output.length >= 5 && memcmp(output.data, "%PDF-", 5) == 0);

    qpdfsharp_free_buffer(output.data);
}

static void protect_then_inspect_with_owner(void)
{
    const char* name = "protect_then_inspect_with_owner";
    pdf_bytes output = { NULL, 0 };
    qpdfsharp_encryption_info info = { 0 };

    CHECK(name, protect_minimal(0, &output) == QPDFSHARP_STATUS_OK);
    CHECK(name, inspect(output, OWNER_PASSWORD, &info) == QPDFSHARP_STATUS_OK);
    CHECK(name, info.is_encrypted == 1);
    CHECK(name, info.revision == 6);
    CHECK(name, info.owner_password_matched == 1);
    CHECK(name, info.permissions == 0);

    qpdfsharp_free_buffer(output.data);
}

static void protect_then_inspect_without_password(void)
{
    const char* name = "protect_then_inspect_without_password";
    pdf_bytes output = { NULL, 0 };
    qpdfsharp_encryption_info info = { 0 };

    CHECK(name, protect_minimal(0, &output) == QPDFSHARP_STATUS_OK);
    CHECK(name, inspect(output, "", &info) == QPDFSHARP_STATUS_OK);
    CHECK(name, info.is_encrypted == 1);
    CHECK(name, info.owner_password_matched == 0);
    CHECK(name, info.user_password_matched == 1);

    qpdfsharp_free_buffer(output.data);
}

static void protect_print_high_reports_both_print_bits(void)
{
    const char* name = "protect_print_high_reports_both_print_bits";
    pdf_bytes output = { NULL, 0 };
    qpdfsharp_encryption_info info = { 0 };

    CHECK(name, protect_minimal(QPDFSHARP_ALLOW_PRINT_HIGH_RESOLUTION, &output) == QPDFSHARP_STATUS_OK);
    CHECK(name, inspect(output, OWNER_PASSWORD, &info) == QPDFSHARP_STATUS_OK);
    CHECK(name, info.permissions == (QPDFSHARP_ALLOW_PRINT_LOW_RESOLUTION | QPDFSHARP_ALLOW_PRINT_HIGH_RESOLUTION));

    qpdfsharp_free_buffer(output.data);
}

static void protect_each_single_flag_round_trips(void)
{
    const char* name = "protect_each_single_flag_round_trips";
    /* PRINT_HIGH_RESOLUTION implies low resolution, so it has its own test above. */
    const uint32_t single_flags[] = {
        QPDFSHARP_ALLOW_PRINT_LOW_RESOLUTION,
        QPDFSHARP_ALLOW_EXTRACT_CONTENT,
        QPDFSHARP_ALLOW_ASSEMBLE_DOCUMENT,
        QPDFSHARP_ALLOW_ANNOTATE_AND_FORM,
        QPDFSHARP_ALLOW_FILL_FORMS,
        QPDFSHARP_ALLOW_MODIFY_CONTENT,
    };

    for (size_t i = 0; i < sizeof single_flags / sizeof single_flags[0]; i++) {
        uint32_t flag = single_flags[i];
        pdf_bytes output = { NULL, 0 };
        qpdfsharp_encryption_info info = { 0 };

        CHECK(name, protect_minimal(flag, &output) == QPDFSHARP_STATUS_OK);
        CHECK(name, inspect(output, OWNER_PASSWORD, &info) == QPDFSHARP_STATUS_OK);
        if (info.permissions != flag) {
            fprintf(stderr, "FAIL: %s: flag 0x%02x inspected as 0x%02x\n", name, flag, info.permissions);
            failures++;
        }

        qpdfsharp_free_buffer(output.data);
    }
}

static void protect_user_password_input_is_password_required(void)
{
    const char* name = "protect_user_password_input_is_password_required";
    pdf_bytes input = read_test_pdf("user-password.pdf");
    uint8_t* output = (uint8_t*)1; /* must be reset to NULL */
    size_t output_length = 1;
    char error_message[ERROR_MESSAGE_CAPACITY] = "";

    qpdfsharp_status status = qpdfsharp_protect_with_owner_password(
        input.data, input.length, OWNER_PASSWORD, 0, &output, &output_length,
        error_message, sizeof error_message);

    CHECK(name, status == QPDFSHARP_STATUS_PASSWORD_REQUIRED);
    CHECK(name, output == NULL);
    CHECK(name, output_length == 0);
    CHECK(name, error_message[0] != '\0');

    free(input.data);
}

static void protect_garbage_is_invalid_pdf(void)
{
    const char* name = "protect_garbage_is_invalid_pdf";
    uint8_t garbage[4096];
    memset(garbage, 0xA5, sizeof garbage);
    uint8_t* output = NULL;
    size_t output_length = 0;
    char error_message[ERROR_MESSAGE_CAPACITY] = "";

    qpdfsharp_status status = qpdfsharp_protect_with_owner_password(
        garbage, sizeof garbage, OWNER_PASSWORD, 0, &output, &output_length,
        error_message, sizeof error_message);

    CHECK(name, status == QPDFSHARP_STATUS_INVALID_PDF);
    CHECK(name, output == NULL);
    CHECK(name, error_message[0] != '\0');
}

static qpdfsharp_status protect_with(
    const uint8_t* input, size_t input_length, const char* owner_password, uint32_t allowed_permissions)
{
    uint8_t* output = NULL;
    size_t output_length = 0;
    char error_message[ERROR_MESSAGE_CAPACITY];
    qpdfsharp_status status = qpdfsharp_protect_with_owner_password(
        input, input_length, owner_password, allowed_permissions, &output, &output_length,
        error_message, sizeof error_message);
    qpdfsharp_free_buffer(output);
    return status;
}

static void protect_rejects_bad_arguments(void)
{
    const char* name = "protect_rejects_bad_arguments";
    pdf_bytes input = read_test_pdf("minimal.pdf");

    char password_128[129];
    memset(password_128, 'a', 128);
    password_128[128] = '\0';

    CHECK(name, protect_with(input.data, 0, OWNER_PASSWORD, 0) == QPDFSHARP_STATUS_INVALID_ARGUMENT);
    CHECK(name, protect_with(input.data, input.length, NULL, 0) == QPDFSHARP_STATUS_INVALID_ARGUMENT);
    CHECK(name, protect_with(input.data, input.length, "", 0) == QPDFSHARP_STATUS_INVALID_ARGUMENT);
    CHECK(name, protect_with(input.data, input.length, password_128, 0) == QPDFSHARP_STATUS_INVALID_ARGUMENT);
    CHECK(name, protect_with(input.data, input.length, OWNER_PASSWORD, 0x80) == QPDFSHARP_STATUS_INVALID_ARGUMENT);

    free(input.data);
}

static void free_buffer_null_is_noop(void)
{
    qpdfsharp_free_buffer(NULL);
}

int main(void)
{
    protect_minimal_ok();
    protect_then_inspect_with_owner();
    protect_then_inspect_without_password();
    protect_print_high_reports_both_print_bits();
    protect_each_single_flag_round_trips();
    protect_user_password_input_is_password_required();
    protect_garbage_is_invalid_pdf();
    protect_rejects_bad_arguments();
    free_buffer_null_is_noop();

    if (failures != 0) {
        fprintf(stderr, "protect_tests: %d failure(s)\n", failures);
        return 1;
    }
    printf("protect_tests: all cases passed\n");
    return 0;
}

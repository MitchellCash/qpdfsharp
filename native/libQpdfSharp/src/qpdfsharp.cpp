#include "qpdfsharp.h"

#include <qpdf/Buffer.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFWriter.hh>

#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>

namespace {

// R6 uses at most the first 127 bytes of a password and silently ignores the rest.
constexpr size_t max_owner_password_length = 127;

// Copies message into error_message, truncated to fit without splitting a UTF-8 sequence.
void write_error_message(char* error_message, size_t capacity, char const* message) noexcept
{
    if (error_message == nullptr || capacity == 0) {
        return;
    }
    size_t length = std::strlen(message);
    if (length >= capacity) {
        length = capacity - 1;
        // Back up to the lead byte of a sequence the cut would split, and drop it too.
        while (length > 0 && (static_cast<unsigned char>(message[length]) & 0xC0) == 0x80) {
            length--;
        }
    }
    std::memcpy(error_message, message, length);
    error_message[length] = '\0';
}

qpdfsharp_status invalid_argument(char* error_message, size_t capacity, char const* message) noexcept
{
    write_error_message(error_message, capacity, message);
    return QPDFSHARP_STATUS_INVALID_ARGUMENT;
}

bool is_owner_password_length_valid(char const* password) noexcept
{
    size_t length = 0;
    while (length <= max_owner_password_length && password[length] != '\0') {
        length++;
    }
    return length >= 1 && length <= max_owner_password_length;
}

// Runs operation and turns every exception it throws into a status and message, so that no
// exception crosses the C boundary.
template <typename Operation>
qpdfsharp_status run_guarded(char* error_message, size_t capacity, Operation&& operation) noexcept
{
    try {
        operation();
        return QPDFSHARP_STATUS_OK;
    } catch (QPDFExc const& e) {
        write_error_message(error_message, capacity, e.what());
        return e.getErrorCode() == qpdf_e_password ? QPDFSHARP_STATUS_PASSWORD_REQUIRED
                                                   : QPDFSHARP_STATUS_INVALID_PDF;
    } catch (std::exception const& e) {
        write_error_message(error_message, capacity, e.what());
        return QPDFSHARP_STATUS_INTERNAL_ERROR;
    } catch (...) {
        write_error_message(error_message, capacity, "unknown error");
        return QPDFSHARP_STATUS_INTERNAL_ERROR;
    }
}

void open_pdf(QPDF& pdf, uint8_t const* input, size_t input_length, char const* password)
{
    pdf.setSuppressWarnings(true);
    // The input buffer must outlive pdf; it does, because pdf is local to the exported call.
    pdf.processMemoryFile("input", reinterpret_cast<char const*>(input), input_length, password);
}

qpdf_r3_print_e print_level(uint32_t allowed_permissions)
{
    if (allowed_permissions & QPDFSHARP_ALLOW_PRINT_HIGH_RESOLUTION) {
        return qpdf_r3p_full;
    }
    if (allowed_permissions & QPDFSHARP_ALLOW_PRINT_LOW_RESOLUTION) {
        return qpdf_r3p_low;
    }
    return qpdf_r3p_none;
}

// Reads back the P bits that setR6EncryptionParameters writes (3, 12, 5, 11, 6, 9 and 4).
uint32_t allowed_permissions_of(QPDF& pdf)
{
    uint32_t permissions = 0;
    if (pdf.allowPrintLowRes()) {
        permissions |= QPDFSHARP_ALLOW_PRINT_LOW_RESOLUTION;
    }
    if (pdf.allowPrintHighRes()) {
        permissions |= QPDFSHARP_ALLOW_PRINT_HIGH_RESOLUTION;
    }
    if (pdf.allowExtractAll()) {
        permissions |= QPDFSHARP_ALLOW_EXTRACT_CONTENT;
    }
    if (pdf.allowModifyAssembly()) {
        permissions |= QPDFSHARP_ALLOW_ASSEMBLE_DOCUMENT;
    }
    if (pdf.allowModifyAnnotation()) {
        permissions |= QPDFSHARP_ALLOW_ANNOTATE_AND_FORM;
    }
    if (pdf.allowModifyForm()) {
        permissions |= QPDFSHARP_ALLOW_FILL_FORMS;
    }
    if (pdf.allowModifyOther()) {
        permissions |= QPDFSHARP_ALLOW_MODIFY_CONTENT;
    }
    return permissions;
}

} // namespace

extern "C" {

const char* qpdfsharp_get_qpdf_version(void)
{
    // QPDFVersion() returns a reference to a static std::string (qpdf's own C API relies on this).
    return QPDF::QPDFVersion().c_str();
}

int32_t qpdfsharp_get_abi_version(void)
{
    return QPDFSHARP_ABI_VERSION;
}

qpdfsharp_status qpdfsharp_protect_with_owner_password(
    const uint8_t* input, size_t input_length,
    const char* owner_password,
    uint32_t allowed_permissions,
    uint8_t** output, size_t* output_length,
    char* error_message, size_t error_message_capacity)
{
    if (output != nullptr) {
        *output = nullptr;
    }
    if (output_length != nullptr) {
        *output_length = 0;
    }
    write_error_message(error_message, error_message_capacity, "");

    if (input == nullptr || owner_password == nullptr || output == nullptr || output_length == nullptr) {
        return invalid_argument(error_message, error_message_capacity,
            "input, owner_password, output and output_length must not be NULL");
    }
    if (input_length == 0) {
        return invalid_argument(error_message, error_message_capacity, "input must not be empty");
    }
    if (!is_owner_password_length_valid(owner_password)) {
        return invalid_argument(error_message, error_message_capacity,
            "owner_password must be 1 to 127 bytes of UTF-8");
    }
    if ((allowed_permissions & ~QPDFSHARP_ALLOW_ALL) != 0) {
        return invalid_argument(error_message, error_message_capacity,
            "allowed_permissions contains bits other than QPDFSHARP_ALLOW_*");
    }

    uint8_t* protected_pdf = nullptr;
    size_t protected_pdf_length = 0;
    qpdfsharp_status status = run_guarded(error_message, error_message_capacity, [&] {
        QPDF pdf;
        open_pdf(pdf, input, input_length, "");

        QPDFWriter writer(pdf);
        writer.setOutputMemory();
        writer.setR6EncryptionParameters(
            "",
            owner_password,
            true, // accessibility: qpdf cannot disable it for R > 3
            (allowed_permissions & QPDFSHARP_ALLOW_EXTRACT_CONTENT) != 0,
            (allowed_permissions & QPDFSHARP_ALLOW_ASSEMBLE_DOCUMENT) != 0,
            (allowed_permissions & QPDFSHARP_ALLOW_ANNOTATE_AND_FORM) != 0,
            (allowed_permissions & QPDFSHARP_ALLOW_FILL_FORMS) != 0,
            (allowed_permissions & QPDFSHARP_ALLOW_MODIFY_CONTENT) != 0,
            print_level(allowed_permissions),
            true); // encrypt_metadata_aes
        writer.write();

        auto const written = writer.getBufferSharedPointer();
        auto* copy = static_cast<uint8_t*>(std::malloc(written->getSize()));
        if (copy == nullptr) {
            throw std::bad_alloc();
        }
        std::memcpy(copy, written->getBuffer(), written->getSize());
        protected_pdf = copy;
        protected_pdf_length = written->getSize();
    });

    if (status == QPDFSHARP_STATUS_OK) {
        *output = protected_pdf;
        *output_length = protected_pdf_length;
    }
    return status;
}

qpdfsharp_status qpdfsharp_inspect_encryption(
    const uint8_t* input, size_t input_length,
    const char* password,
    qpdfsharp_encryption_info* info,
    char* error_message, size_t error_message_capacity)
{
    if (info != nullptr) {
        *info = qpdfsharp_encryption_info{};
    }
    write_error_message(error_message, error_message_capacity, "");

    if (input == nullptr || password == nullptr || info == nullptr) {
        return invalid_argument(error_message, error_message_capacity,
            "input, password and info must not be NULL");
    }
    if (input_length == 0) {
        return invalid_argument(error_message, error_message_capacity, "input must not be empty");
    }

    return run_guarded(error_message, error_message_capacity, [&] {
        QPDF pdf;
        open_pdf(pdf, input, input_length, password);

        qpdfsharp_encryption_info result{};
        int revision = 0;
        int p_value = 0;
        if (pdf.isEncrypted(revision, p_value)) {
            result.is_encrypted = 1;
            result.revision = revision;
            result.permissions = allowed_permissions_of(pdf);
        } else {
            result.permissions = QPDFSHARP_ALLOW_ALL;
        }
        result.owner_password_matched = pdf.ownerPasswordMatched() ? 1 : 0;
        result.user_password_matched = pdf.userPasswordMatched() ? 1 : 0;
        *info = result;
    });
}

void qpdfsharp_free_buffer(uint8_t* buffer)
{
    std::free(buffer);
}

} // extern "C"

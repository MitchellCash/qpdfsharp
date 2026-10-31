#include "qpdfsharp.h"

#include <qpdf/QPDF.hh>

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

} // extern "C"

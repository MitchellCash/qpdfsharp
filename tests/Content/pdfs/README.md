# Test PDFs

Shared by the native tests (`native/libQpdfSharp/tests/`) and the managed tests
(`tests/QpdfSharp.Tests/`). They were generated with the `qpdf` 12.4.2 CLI; the library never
shells out to it.

## `minimal.pdf`

A one-page Letter-size PDF that draws "QpdfSharp" in Helvetica. It was hand-written as
`minimal-src.pdf` (below, with no xref table) and then normalised by qpdf, which reconstructs the
cross-reference table:

```bash
qpdf minimal-src.pdf tests/Content/pdfs/minimal.pdf
```

qpdf exits with status 3 and warns that it reconstructed the missing xref; that is expected.

`minimal-src.pdf`:

```
%PDF-1.7
1 0 obj
<< /Type /Catalog /Pages 2 0 R >>
endobj
2 0 obj
<< /Type /Pages /Kids [3 0 R] /Count 1 >>
endobj
3 0 obj
<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>
endobj
4 0 obj
<< /Length 40 >>
stream
BT /F1 12 Tf 72 720 Td (QpdfSharp) Tj ET
endstream
endobj
5 0 obj
<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>
endobj
trailer
<< /Size 6 /Root 1 0 R >>
%%EOF
```

## `user-password.pdf`

`minimal.pdf` encrypted with AES-256 (R6), user password `user-secret` and owner password
`owner-secret`. It cannot be opened without a password, so protecting it must fail with
"password required":

```bash
qpdf --encrypt --user-password=user-secret --owner-password=owner-secret --bits=256 -- \
  tests/Content/pdfs/minimal.pdf tests/Content/pdfs/user-password.pdf
```

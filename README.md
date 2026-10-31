# QpdfSharp

QpdfSharp is a .NET library that protects PDF files with an owner password, using a statically linked
build of [qpdf](https://github.com/qpdf/qpdf) behind a small C API. Anyone can open a protected file;
the owner password is needed to lift the restrictions. The project is laid out like
[SkiaSharp](https://github.com/mono/SkiaSharp). It is in early development: see `AGENTS.md` for the
repository layout and build commands.

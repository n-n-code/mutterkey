# Third-Party Notices

This repository includes vendored third-party source code.

## whisper.cpp

- Upstream project: `ggml-org/whisper.cpp`
- Upstream URL: <https://github.com/ggml-org/whisper.cpp>
- Vendored location in this repository: `third_party/whisper.cpp`
- How it is used here: built in-process and linked into `mutterkey`

Primary upstream license:

- `third_party/whisper.cpp/LICENSE`
- License identifier: `MIT`

Additional file-level notices present in the vendored snapshot:

- Some SYCL-related files include LLVM Apache-2.0-with-LLVM-exception notices in
  addition to MIT/Intel notices. Example:
  `third_party/whisper.cpp/ggml/src/ggml-sycl/convert.hpp`
- The vendored `FindFFmpeg.cmake` file carries a BSD-license notice in its file
  header and references `COPYING-CMAKE-SCRIPTS`. Example:
  `third_party/whisper.cpp/cmake/FindFFmpeg.cmake`

If you redistribute this repository, keep the original third-party license files
and file-level notices intact.

## System dependencies

Mutterkey also depends on Qt 6 and KDE Frameworks at build and runtime. Those
libraries are not vendored in this repository; they are provided by the user's
system packages and remain under their own licenses.

## Speech models

This repository does not include Whisper model files. Any model file you
download separately may be subject to its own license or usage terms.

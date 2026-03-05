# MRTKLIB : Modernized RTKLIB for Next-Generation GNSS

[![License](https://img.shields.io/badge/License-BSD_2--Clause-blue.svg)](LICENSE.txt)
[![Build](https://img.shields.io/badge/build-CMake-success.svg)]()
[![C11](https://img.shields.io/badge/standard-C11-blue.svg)]()

**MRTKLIB** is a completely modernized, thread-safe, and modularized C11 library for standard and precise GNSS positioning.

It is designed to overcome the architectural limitations of the original legacy [RTKLIB](https://www.rtklib.com/), providing a robust foundation for next-generation GNSS applications, including high-scale server processing, containerized environments, and seamless integration of Japanese QZSS augmentation services.

The structural foundation is based on **MALIB (MADOCA-PPP Library) feature/1.2.0** developed by JAXA and TOSHIBA, with the PPP positioning engine replaced by **MADOCALIB**'s implementation for higher-accuracy PPP/PPP-AR processing. Both post-processing (`rnx2rtkp`) and real-time processing (`rtkrcv`) are supported, including L6E (SSR orbit/clock/bias) and L6D (ionospheric STEC) correction streams.

---

## 🚀 Key Architectural Overhauls (Departure from Legacy)

MRTKLIB is not just another fork; it is a ground-up architectural redesign aimed at modern software engineering standards:

* **Thread-Safe Design:** Introduced the `mrtk_ctx_t` context structure to encapsulate states, error handling, and logging on a per-instance basis, replacing legacy global variables in the major processing pipelines and enabling parallel processing (e.g., handling multiple streams in a single server).
* **POSIX & C11 Pure:** Purged all Win32 API and legacy `#ifdef` macros. The core library is purely POSIX/C11 compliant, ensuring perfect portability across Linux, macOS (Apple Silicon), and embedded ARM/RISC-V devices.
* **Modern Build System (CMake):** Replaced scattered Makefiles with a unified, clean CMake build system. It natively supports standard `find_package(LAPACK)` for hardware-accelerated matrix operations (replacing bundled proprietary Intel MKL binaries).
* **Domain-Driven Directory Structure:** Flat source files have been beautifully categorized into specific domains (`src/core/`, `src/pos/`, `src/rtcm/`, `src/models/`, etc.) for high maintainability.

## 🗺️ Roadmap: The QZSS Grand Integration

The ultimate goal is to unify the fragmented QZSS augmentation ecosystem into a single, conflict-free library. Current integration status:

| Component | Version | Description | Status |
|-----------|---------|-------------|--------|
| **MALIB** | feature/1.2.0 (`f006a34`) | MADOCA-PPP structural base (directory layout, threading, streams) | Integrated |
| **MADOCALIB** | ver.2.0 (`8091004`) | PPP/PPP-AR engine, L6E SSR decoder, L6D ionospheric decoder | Integrated |
| **CLASLIB** | ver.0.8.2 (`9e714b9`) | Centimeter Level Augmentation Service (PPP-RTK, VRS-RTK, CSSR decoder) | Integrated |

With the MADOCALIB integration complete, users can process L6E (orbit/clock/bias corrections) and L6D (ionospheric STEC corrections) streams seamlessly in both post-processing and real-time modes.

### Known Limitations

| Mode | L6E (SSR) | L6D (Ionospheric) | Notes |
|------|-----------|-------------------|-------|
| **Post-processing** (`rnx2rtkp`) | Multiple `.l6` files | Multiple `.l6` files | Full PPP/PPP-AR/PPP-AR+iono support |
| **Real-time** (`rtkrcv`) | Single stream (`inpstr3`) | Not yet supported | PPP/PPP-AR only |

* **Real-time correction stream**: The rtksvr architecture provides a single correction input (`inpstr3`). Multiple QZSS L6E channels (e.g., QZS-3 and QZS-4) are supported when the receiver multiplexes them into one SBF stream, which is the typical configuration for Septentrio and similar receivers.
* **Real-time L6D**: Ionospheric STEC correction (L6D) input is not yet available in real-time mode. PPP-AR+iono requires post-processing.

---

## 🛠️ Getting Started (How to Build)

### Prerequisites
* CMake (>= 3.15)
* C11 compatible compiler (GCC, Clang)
* LAPACK/BLAS (Optional but recommended for fast matrix operations. e.g., `liblapack-dev` on Ubuntu, Accelerate framework on macOS)

### Build Instructions
MRTKLIB uses standard CMake workflow. Out-of-source builds are strictly recommended.

```bash
# Clone the repository
git clone https://github.com/h-shiono/MRTKLIB.git
cd MRTKLIB

# Configure the project
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build the library and apps
cmake --build build -j
```

Compiled applications (e.g., rnx2rtkp, rtkrcv) will be located in the `build/` directory.

### Running Applications
Configuration files and templates are stored in the `conf/` directory.
Test data and regression datasets are available in `tests/data/`.

```bash
# Example: Running post-processing analysis
./build/rnx2rtkp -k conf/malib/rnx2rtkp.conf tests/data/rtklib/rinex/xxxx.obs ...
```

## 👨‍💻 For Developers
### Running Tests
MRTKLIB includes both robust unit tests (utest) and regression tests to ensure core stability when merging complex GNSS algorithms.

```bash
cd build
ctest --output-on-failure
```

### Generating Documentation
The codebase is fully documented using Doxygen. To generate the API reference and call graphs:

```bash
cmake --build build --target doc
```
Documentation will be generated in `build/doc/html/index.html`.

## 📄 License & Attributions
MRTKLIB is distributed under the BSD 2-Clause License.

This project stands on the shoulders of giants:

| Contributor | Role |
|-------------|------|
| **T. Takasu** | RTKLIB — the foundational GNSS positioning library |
| **T. Suzuki** | RTKLIB — u-blox receiver decoder |
| **Geospatial Information Authority of Japan** | GSILIB v1.0.3 — CLAS grid correction algorithms |
| **Mitsubishi Electric Corp.** | CLASLIB — CLAS PPP-RTK / VRS-RTK engine |
| **Japan Aerospace Exploration Agency** | MALIB — MADOCA-PPP structural base |
| **TOSHIBA ELECTRONIC TECHNOLOGIES** | MALIB + MADOCALIB — MADOCA-PPP engine and L6E/L6D decoder |
| **Cabinet Office, Japan** | MADOCALIB — PPP/PPP-AR positioning algorithms |
| **Lighthouse Technology & Consulting** | MADOCALIB — system integration and L6E/L6D decoder |

For detailed licensing information, please refer to [LICENSE.txt](LICENSE.txt).

# Installation

## Prerequisites

| Requirement | Version | Notes |
|-------------|---------|-------|
| C compiler | C11 support | GCC 7+, Clang 5+, Apple Clang |
| CMake | 3.15+ | Build system |
| LAPACK | any | Hardware-accelerated matrix operations |

### Platform-Specific Dependencies

=== "Ubuntu / Debian"

    ```bash
    sudo apt-get update
    sudo apt-get install -y cmake build-essential liblapack-dev
    ```

=== "macOS (Homebrew)"

    ```bash
    # Xcode command line tools provide Apple Clang + Accelerate (LAPACK)
    xcode-select --install
    brew install cmake
    ```

=== "macOS (Apple Silicon)"

    No additional LAPACK installation needed — macOS ships with the Accelerate framework which provides LAPACK.

## Build from Source

```bash
git clone https://github.com/h-shiono/MRTKLIB.git
cd MRTKLIB

# Configure
cmake --preset default

# Build
cmake --build build
```

The build produces a single binary `build/mrtk` with all subcommands.

## Verify Installation

```bash
# Check version
build/mrtk --version

# Show available subcommands
build/mrtk --help
```

Expected output:

```
mrtk: MRTKLIB unified CLI (MRTKLIB ver.0.6.2)

Usage: mrtk [COMMAND] [OPTIONS]

Core Commands:
  run         Run real-time positioning pipeline (rtkrcv)
  post        Run post-processing positioning (rnx2rtkp)

Data & Streaming:
  relay       Relay and split data streams (str2str)
  convert     Convert receiver raw data to RINEX (convbin)

Format Translation:
  ssr2obs     Convert SSR corrections to pseudo-observations
  ssr2osr     Convert SSR corrections to OSR

Utilities:
  bias        Estimate receiver fractional biases
  dump        Dump stream data to human-readable format (e.g., 'mrtk dump cssr')
```

## Run Tests

```bash
cd build
ctest --output-on-failure
```

!!! note "Real-time tests"
    Tests labeled `realtime` use file-stream replay (ranging from ~90 seconds to several minutes) and are excluded from CI.
    To run them locally:

    ```bash
    ctest --output-on-failure -L realtime
    ```

## Next Steps

- [First Run](first-run.md) — Run your first positioning solution
- [CLI Reference](../guide/cli.md) — Learn all `mrtk` subcommands
- [Configuration](../guide/configuration.md) — Set up TOML configuration files

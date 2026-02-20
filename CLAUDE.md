# QZSSLIB Migration & Development Guide

## 1. Project Overview
This project, "QZSSLIB," aims to modernize and integrate GNSS positioning libraries (specifically JAXA's MALIB, claslib, and madocalib) into a single, cohesive, and modern C/C++ software package.
The project is currently in the **Incremental Migration Phase**. We are moving from a legacy RTKLIB-based C structure to a modern CMake/vcpkg-based architecture without breaking the core mathematical and physical logic.

## 2. Your Role (AI Assistant)
Your primary responsibilities are:
1. **Migration Verification:** Ensure that C code ported to the new directory structure (`include/mrtklib/` and `src/`) maintains strict logical equivalence to the original MALIB code.
2. **Build & Compiler Support:** Assist in resolving CMake configuration issues, vcpkg dependency problems, and C/C++ compiler errors (especially regarding include paths and linkage).
3. **Documentation:** Generate and maintain high-quality Doxygen-style docstrings for all functions, structs, and classes.
4. **Refactoring:** When requested, help refactor legacy C code (e.g., massive structs, global variables) into modern, safe C++ concepts, but **only after** regression tests for the current state are passing.

## 3. Directory Architecture
Understand the target directory structure:
- `apps/`: Executable entry points (e.g., CLI, GUI). Separated from core logic.
- `include/mrtklib/`: Public headers.
- `src/`: Core implementation files (`mrtk_*.c` or `.cpp`).
- `tests/`: CTest-based regression and unit tests.
- `vcpkg/`: Dependency management.

## 4. Coding Standards & Rules
- **No Mathematical Changes:** Do NOT alter any GNSS algorithms, matrix operations, or physical constants during the structural porting phase unless explicitly instructed.
- **Docstrings:** Use strict Doxygen format for all headers and function definitions.
  ```cpp
  /**
   * @brief Short description of the function.
   * @param[in]  param_name Description of input parameter.
   * @param[out] param_name Description of output parameter.
   * @return Return value description.
   */
  ```

* **Modernization (C to C++):** When transitioning files to C++, prefer `std::vector` over raw dynamic arrays, use smart pointers for resource management, and apply `const` correctness rigorously. However, maintain `extern "C"` compatibility for headers that are still included by legacy C files.
* **Formatting:** Assume `.clang-format` is in use. Keep code layout clean, standard, and readable.

## 5. Development Workflow (Incremental)

Whenever assisting with a code change, adhere to this loop:

1. **Port/Refactor:** Make the minimal required change to move a component or update a struct.
2. **Compile:** Ensure `CMake` configures and builds successfully.
3. **Test:** Remind the user to run `ctest`. Never proceed to the next major refactoring step without confirming that the regression tests (Golden Data matching) have passed.

## 6. Common Build Commands (for context)

* Configure: `cmake --preset default` (or similar depending on CMakePresets.json)
* Build: `cmake --build build`
* Test: `cd build && ctest --output-on-failure`

# compare_bia.cmake
# Compare two Bias SINEX (.bia) files by their bias data lines only.
# Only lines starting with ' OSB' or ' DSB' (actual bias values) are compared.
# Header markers (%=BIA), block markers (+/-), and comment lines (*) are skipped
# as they may contain version strings and paths that differ between builds.
#
# Usage (cmake -P mode):
#   cmake -DOUTPUT=<out.bia> -DREFERENCE=<ref.bia> -P compare_bia.cmake
#   cmake -DOUTPUT=<out.bia> -DREFERENCE=<ref.bia> -DTHRESHOLD=90 -P compare_bia.cmake
#
# THRESHOLD (optional, default 100):
#   When 100  → exact line-by-line comparison (same platform, bit-for-bit).
#   When < 100 → only checks that output line count >= THRESHOLD% of reference.
#               Use this for cross-platform CI where floating-point may differ.

foreach(var OUTPUT REFERENCE)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "compare_bia.cmake: ${var} is not defined")
    endif()
    if(NOT EXISTS "${${var}}")
        message(FATAL_ERROR "compare_bia.cmake: file not found: ${${var}}")
    endif()
endforeach()

if(NOT DEFINED THRESHOLD)
    set(THRESHOLD 100)
endif()

# Read and filter bias data lines (OSB/DSB lines only)
foreach(tag OUTPUT REFERENCE)
    file(STRINGS "${${tag}}" all_lines)
    set(data_lines "")
    foreach(line IN LISTS all_lines)
        if(line MATCHES "^ (OSB|DSB) ")
            list(APPEND data_lines "${line}")
        endif()
    endforeach()
    set(${tag}_DATA "${data_lines}")
endforeach()

list(LENGTH OUTPUT_DATA   out_len)
list(LENGTH REFERENCE_DATA ref_len)

if(out_len EQUAL 0)
    message(FATAL_ERROR "compare_bia.cmake: output has no bias data lines: ${OUTPUT}")
endif()

if(THRESHOLD EQUAL 100)
    # Exact comparison
    if(NOT out_len EQUAL ref_len)
        message(FATAL_ERROR
            "compare_bia.cmake: line count mismatch\n"
            "  output   : ${out_len} bias data lines  (${OUTPUT})\n"
            "  reference: ${ref_len} bias data lines  (${REFERENCE})")
    endif()
    if(NOT OUTPUT_DATA STREQUAL REFERENCE_DATA)
        message(FATAL_ERROR
            "compare_bia.cmake: data mismatch between\n"
            "  output   : ${OUTPUT}\n"
            "  reference: ${REFERENCE}")
    endif()
    message(STATUS "compare_bia: OK  (${out_len} bias data lines match exactly)")
else()
    # Threshold comparison — used for cross-platform CI
    math(EXPR threshold_lines "${ref_len} * ${THRESHOLD} / 100")
    message(STATUS
        "compare_bia: lines  output=${out_len}  reference=${ref_len}"
        "  threshold=${threshold_lines} (${THRESHOLD}%)")
    if(out_len LESS threshold_lines)
        message(FATAL_ERROR
            "compare_bia.cmake: too few bias data lines\n"
            "  output   : ${out_len}\n"
            "  threshold: ${threshold_lines} (${THRESHOLD}% of ${ref_len})\n"
            "  file     : ${OUTPUT}")
    endif()
    message(STATUS "compare_bia: OK  (${out_len} >= ${threshold_lines} lines, ${THRESHOLD}% threshold)")
endif()

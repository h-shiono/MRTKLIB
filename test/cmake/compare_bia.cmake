# compare_bia.cmake
# Compare two Bias SINEX (.bia) files by their bias data lines only.
# Only lines starting with ' OSB' or ' DSB' (actual bias values) are compared.
# Header markers (%=BIA), block markers (+/-), and comment lines (*) are skipped
# as they may contain version strings and paths that differ between builds.
#
# Usage (cmake -P mode):
#   cmake -DOUTPUT=<out.bia> -DREFERENCE=<ref.bia> -P compare_bia.cmake

foreach(var OUTPUT REFERENCE)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "compare_bia.cmake: ${var} is not defined")
    endif()
    if(NOT EXISTS "${${var}}")
        message(FATAL_ERROR "compare_bia.cmake: file not found: ${${var}}")
    endif()
endforeach()

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

message(STATUS "compare_bia: OK  (${out_len} bias data lines match)")

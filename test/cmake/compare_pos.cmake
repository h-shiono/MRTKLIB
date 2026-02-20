# compare_pos.cmake
# Compare two RTKLIB .pos files by their data lines only.
# Lines starting with '%' are header/comment lines and are intentionally
# skipped — they contain version strings, input paths, and options that
# may legitimately differ between builds.
#
# Usage (cmake -P mode):
#   cmake -DOUTPUT=<out.pos> -DREFERENCE=<ref.pos> -P compare_pos.cmake

foreach(var OUTPUT REFERENCE)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "compare_pos.cmake: ${var} is not defined")
    endif()
    if(NOT EXISTS "${${var}}")
        message(FATAL_ERROR "compare_pos.cmake: file not found: ${${var}}")
    endif()
endforeach()

# Read and filter data lines (exclude '%' comment/header lines)
foreach(tag OUTPUT REFERENCE)
    file(STRINGS "${${tag}}" all_lines)
    set(data_lines "")
    foreach(line IN LISTS all_lines)
        if(NOT line MATCHES "^[ \t]*%")
            list(APPEND data_lines "${line}")
        endif()
    endforeach()
    set(${tag}_DATA "${data_lines}")
endforeach()

list(LENGTH OUTPUT_DATA   out_len)
list(LENGTH REFERENCE_DATA ref_len)

if(out_len EQUAL 0)
    message(FATAL_ERROR "compare_pos.cmake: output has no data lines: ${OUTPUT}")
endif()

if(NOT out_len EQUAL ref_len)
    message(FATAL_ERROR
        "compare_pos.cmake: line count mismatch\n"
        "  output   : ${out_len} data lines  (${OUTPUT})\n"
        "  reference: ${ref_len} data lines  (${REFERENCE})")
endif()

if(NOT OUTPUT_DATA STREQUAL REFERENCE_DATA)
    message(FATAL_ERROR
        "compare_pos.cmake: data mismatch between\n"
        "  output   : ${OUTPUT}\n"
        "  reference: ${REFERENCE}")
endif()

message(STATUS "compare_pos: OK  (${out_len} data lines match)")

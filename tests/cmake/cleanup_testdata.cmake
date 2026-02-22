# cleanup_testdata.cmake
# Removes test data files used by regression tests.
# Called as a CTest FIXTURES_CLEANUP script.
#
# Usage:
#   cmake -DTESTDATA_DIR=<path/to/data> -P cleanup_testdata.cmake

if(NOT DEFINED TESTDATA_DIR)
    message(FATAL_ERROR "cleanup_testdata.cmake: TESTDATA_DIR is not defined")
endif()

set(extracted_files
    "${TESTDATA_DIR}/2024235L.209.l6"
    "${TESTDATA_DIR}/MALIB_OSS_data_obsnav_240822-1100.sbf"
    "${TESTDATA_DIR}/MALIB_OSS_data_obsnav_240822-1100.sbf.tag"
    "${TESTDATA_DIR}/MALIB_OSS_data_obsnav_240822-1100.obs"
    "${TESTDATA_DIR}/MALIB_OSS_data_obsnav_240822-1100.nav"
    "${TESTDATA_DIR}/MALIB_OSS_data_l6e_240822-1100.sbf"
    "${TESTDATA_DIR}/MALIB_OSS_data_l6e_240822-1100.sbf.tag"
    "${TESTDATA_DIR}/igs14_20230719_KMD_add.atx"
    "${TESTDATA_DIR}/COD0OPSFIN_20242350000_01D_01H_GIM.INX"
)

foreach(f IN LISTS extracted_files)
    if(EXISTS "${f}")
        file(REMOVE "${f}")
        message(STATUS "cleanup_testdata: removed ${f}")
    endif()
endforeach()

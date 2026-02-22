# download_tec.cmake
# Downloads IONEX TEC file from CODE (Univ. of Bern) if not present.
# Called as a CTest FIXTURES_SETUP script.
#
# Usage:
#   cmake -DTESTDATA_DIR=<path/to/data> -P download_tec.cmake

if(NOT DEFINED TESTDATA_DIR)
    message(FATAL_ERROR "download_tec.cmake: TESTDATA_DIR is not defined")
endif()

set(TEC_BASENAME "COD0OPSFIN_20242350000_01D_01H_GIM.INX")
set(TEC_FILE "${TESTDATA_DIR}/${TEC_BASENAME}")
set(TEC_URL  "http://ftp.aiub.unibe.ch/CODE/2024/${TEC_BASENAME}.gz")
set(TEC_GZ   "${TEC_FILE}.gz")

if(EXISTS "${TEC_FILE}")
    message(STATUS "download_tec: already present ${TEC_FILE}")
    return()
endif()

message(STATUS "download_tec: downloading ${TEC_URL}")
file(DOWNLOAD "${TEC_URL}" "${TEC_GZ}"
    STATUS dl_status
    SHOW_PROGRESS
)
list(GET dl_status 0 dl_rc)
list(GET dl_status 1 dl_msg)

if(NOT dl_rc EQUAL 0)
    file(REMOVE "${TEC_GZ}")
    message(FATAL_ERROR
        "download_tec: download failed (${dl_rc}: ${dl_msg})\n"
        "  URL: ${TEC_URL}")
endif()

# Decompress .gz -> .INX
execute_process(
    COMMAND gzip -dc "${TEC_GZ}"
    OUTPUT_FILE "${TEC_FILE}"
    RESULT_VARIABLE gz_rc
)
file(REMOVE "${TEC_GZ}")

if(NOT gz_rc EQUAL 0)
    file(REMOVE "${TEC_FILE}")
    message(FATAL_ERROR "download_tec: decompression failed (exit code ${gz_rc})")
endif()

message(STATUS "download_tec: OK ${TEC_FILE}")

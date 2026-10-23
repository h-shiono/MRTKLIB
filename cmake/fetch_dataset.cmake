# fetch_dataset.cmake : fetch, verify and extract one test dataset (#386)
#
# Script mode (cmake -P). Datasets are release assets of h-shiono/mrtklib-data,
# pinned by SHA256 in tests/data/datasets.cmake.
#
#   -DDATASET=<name-vN>   asset <name-vN>.tar.gz of release tag <name-vN>
#   -DSHA256=<hex>        expected SHA256 of the asset
#   -DURL_BASE=<url>      release download base (".../releases/download")
#   -DCACHE_DIR=<dir>     download cache ("" = $XDG_CACHE_HOME or ~/.cache,
#                         subdirectory mrtklib-data)
#   -DLOCAL_DIR=<dir>     pre-downloaded assets, checked before the cache and
#                         the network ("" = none)
#   -DDEST=<dir>          extract into this directory ("" = fetch only)
#
# A cached or local asset with the right SHA256 is used without network
# access. Downloads go to a temporary name and are renamed into the cache
# only after the hash check, so concurrent ctest fixtures cannot see a
# partial file.

foreach(_var DATASET SHA256 URL_BASE)
    if(NOT ${_var})
        message(FATAL_ERROR "fetch_dataset: ${_var} is required")
    endif()
endforeach()

set(_asset "${DATASET}.tar.gz")
string(TOLOWER "${SHA256}" _want)

if(NOT CACHE_DIR)
    if(DEFINED ENV{XDG_CACHE_HOME} AND NOT "$ENV{XDG_CACHE_HOME}" STREQUAL "")
        set(CACHE_DIR "$ENV{XDG_CACHE_HOME}/mrtklib-data")
    elseif(DEFINED ENV{HOME})
        set(CACHE_DIR "$ENV{HOME}/.cache/mrtklib-data")
    else()
        message(FATAL_ERROR "fetch_dataset: no cache directory (set MRTK_TEST_DATA_CACHE)")
    endif()
endif()

# returns TRUE in ${out} if ${file} exists and matches the expected hash
function(_check_hash file out)
    set(${out} FALSE PARENT_SCOPE)
    if(EXISTS "${file}")
        file(SHA256 "${file}" _got)
        if(_got STREQUAL _want)
            set(${out} TRUE PARENT_SCOPE)
        else()
            message(WARNING "${DATASET}: ${file} has SHA256 ${_got}, expected ${_want}; ignored")
        endif()
    endif()
endfunction()

set(_file "")
if(LOCAL_DIR)
    _check_hash("${LOCAL_DIR}/${_asset}" _ok)
    if(_ok)
        set(_file "${LOCAL_DIR}/${_asset}")
    endif()
endif()

if(NOT _file)
    set(_cached "${CACHE_DIR}/${_asset}")
    _check_hash("${_cached}" _ok)
    if(NOT _ok)
        file(MAKE_DIRECTORY "${CACHE_DIR}")
        string(RANDOM LENGTH 8 _rnd)
        set(_tmp "${_cached}.part-${_rnd}")
        set(_url "${URL_BASE}/${DATASET}/${_asset}")
        message(STATUS "${DATASET}: downloading ${_url}")
        file(DOWNLOAD "${_url}" "${_tmp}" STATUS _st TLS_VERIFY ON)
        list(GET _st 0 _code)
        if(NOT _code EQUAL 0)
            list(GET _st 1 _msg)
            file(REMOVE "${_tmp}")
            message(FATAL_ERROR
                "${DATASET}: download failed (${_msg}): ${_url}\n"
                "Offline? Put ${_asset} in a directory and configure with "
                "-DMRTK_TEST_DATA_DIR=<dir>.")
        endif()
        file(SHA256 "${_tmp}" _got)
        if(NOT _got STREQUAL _want)
            file(REMOVE "${_tmp}")
            message(FATAL_ERROR
                "${DATASET}: SHA256 mismatch for ${_url}\n  got      ${_got}\n  expected ${_want}")
        endif()
        file(RENAME "${_tmp}" "${_cached}")
    endif()
    set(_file "${_cached}")
endif()

if(DEST)
    file(MAKE_DIRECTORY "${DEST}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar xzf "${_file}"
        WORKING_DIRECTORY "${DEST}"
        RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "${DATASET}: extracting ${_file} into ${DEST} failed (${_rc})")
    endif()
    message(STATUS "${DATASET}: extracted into ${DEST}")
else()
    message(STATUS "${DATASET}: ${_file}")
endif()

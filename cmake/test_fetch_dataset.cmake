# test_fetch_dataset.cmake : self-test of cmake/fetch_dataset.cmake (#386)
#
# Script mode, no network: a tiny asset is served through a file:// URL.
#   -DFETCH=<path to fetch_dataset.cmake>  -DWORK=<scratch directory>
#
# Checks: download into an empty cache and extract; warm cache works with an
# unreachable URL (offline); with valid copies in both LOCAL_DIR and the cache,
# the LOCAL_DIR copy is the one used; a wrong SHA256 fails and leaves no file
# in the cache.

file(REMOVE_RECURSE "${WORK}")
set(_name "selftest-v1")
set(_srv "${WORK}/server/${_name}")
file(MAKE_DIRECTORY "${_srv}" "${WORK}/src")
file(WRITE "${WORK}/src/payload.txt" "mrtklib dataset fetch self-test\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar czf "${_srv}/${_name}.tar.gz" payload.txt
    WORKING_DIRECTORY "${WORK}/src")
file(SHA256 "${_srv}/${_name}.tar.gz" _sha)

set(_fails 0)
# run fetch_dataset.cmake; expect success (1) or failure (0). The output is
# returned in _RUN_OUT (fetch-only runs print the path of the asset used).
function(_run label expect url cache local dest sha)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -DDATASET=${_name} -DSHA256=${sha} -DURL_BASE=${url}
            "-DCACHE_DIR=${cache}" "-DLOCAL_DIR=${local}" "-DDEST=${dest}" -P "${FETCH}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(_RUN_OUT "${_out}${_err}" PARENT_SCOPE)
    if((_rc EQUAL 0) EQUAL expect)
        message(STATUS "ok  : ${label}")
    else()
        message(STATUS "FAIL: ${label} (rc=${_rc})\n${_out}${_err}")
        math(EXPR _f "${_fails} + 1")
        set(_fails ${_f} PARENT_SCOPE)
    endif()
endfunction()

set(_url "file://${WORK}/server")
set(_cache "${WORK}/cache")

_run("download into an empty cache and extract" 1 "${_url}" "${_cache}" "" "${WORK}/out1" ${_sha})
if(NOT EXISTS "${_cache}/${_name}.tar.gz" OR NOT EXISTS "${WORK}/out1/payload.txt")
    message(STATUS "FAIL: cache file or extracted payload missing")
    math(EXPR _fails "${_fails} + 1")
endif()

_run("warm cache, unreachable URL (offline)" 1 "file://${WORK}/nowhere" "${_cache}" "" "${WORK}/out2" ${_sha})

file(MAKE_DIRECTORY "${WORK}/local")
file(COPY "${_srv}/${_name}.tar.gz" DESTINATION "${WORK}/local")
_run("LOCAL_DIR without cache or network" 1 "file://${WORK}/nowhere" "${WORK}/cache-empty" "${WORK}/local"
    "${WORK}/out3" ${_sha})

# valid copies in both LOCAL_DIR and the (warm) cache: LOCAL_DIR must win
_run("LOCAL_DIR before a warm cache" 1 "${_url}" "${_cache}" "${WORK}/local" "" ${_sha})
if(NOT _RUN_OUT MATCHES "${_name}: ${WORK}/local/${_name}.tar.gz")
    message(STATUS "FAIL: LOCAL_DIR copy not used:\n${_RUN_OUT}")
    math(EXPR _fails "${_fails} + 1")
endif()
# and with LOCAL_DIR unset, the cache copy is used (no download)
_run("warm cache used when LOCAL_DIR is unset" 1 "file://${WORK}/nowhere" "${_cache}" "" "" ${_sha})
if(NOT _RUN_OUT MATCHES "${_name}: ${_cache}/${_name}.tar.gz")
    message(STATUS "FAIL: cache copy not used:\n${_RUN_OUT}")
    math(EXPR _fails "${_fails} + 1")
endif()

string(REPEAT "0" 64 _bad)
_run("wrong SHA256 fails" 0 "${_url}" "${WORK}/cache-bad" "" "" ${_bad})
file(GLOB _left "${WORK}/cache-bad/*")
if(_left)
    message(STATUS "FAIL: a rejected download was left in the cache: ${_left}")
    math(EXPR _fails "${_fails} + 1")
endif()

if(_fails GREATER 0)
    message(FATAL_ERROR "${_fails} fetch_dataset check(s) failed")
endif()
message(STATUS "all fetch_dataset checks passed")

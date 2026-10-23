# MrtkTestData.cmake : test datasets hosted as release assets (#386)
#
# Large test datasets are not committed to this repository. Each one is a
# release asset <name-vN>.tar.gz of h-shiono/mrtklib-data, pinned by SHA256 in
# tests/data/datasets.cmake, and fetched by the ctest fixture that needs it
# (cmake/fetch_dataset.cmake). Configure and build never touch the network.
#
#   mrtk_dataset(<name-vN> SHA256 <hex>)
#       Register a dataset (tests/data/datasets.cmake).
#   mrtk_dataset_setup_test(<test> <name-vN> <dest-dir>)
#       Add a ctest that fetches, verifies and extracts the dataset into
#       <dest-dir>; give it FIXTURES_SETUP like any other setup test.
#   fetch-test-data (build target)
#       Fetch every registered dataset into the cache in advance.

set(MRTK_TEST_DATA_URL_BASE "https://github.com/h-shiono/mrtklib-data/releases/download"
    CACHE STRING "Download base of the test dataset release assets")
set(MRTK_TEST_DATA_CACHE ""
    CACHE PATH "Test dataset download cache (empty: \$XDG_CACHE_HOME or ~/.cache, /mrtklib-data)")
set(MRTK_TEST_DATA_DIR ""
    CACHE PATH "Directory of pre-downloaded test dataset assets, used before the network")

set(_MRTK_FETCH_DATASET ${CMAKE_CURRENT_LIST_DIR}/fetch_dataset.cmake)
add_custom_target(fetch-test-data)

function(mrtk_dataset name)
    cmake_parse_arguments(PARSE_ARGV 1 _ds "" "SHA256" "")
    if(NOT _ds_SHA256)
        message(FATAL_ERROR "mrtk_dataset(${name}): SHA256 is required")
    endif()
    set_property(GLOBAL PROPERTY MRTK_DATASET_SHA256_${name} ${_ds_SHA256})
    add_custom_command(TARGET fetch-test-data POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDATASET=${name} -DSHA256=${_ds_SHA256}
            -DURL_BASE=${MRTK_TEST_DATA_URL_BASE}
            "-DCACHE_DIR=${MRTK_TEST_DATA_CACHE}" "-DLOCAL_DIR=${MRTK_TEST_DATA_DIR}"
            -P ${_MRTK_FETCH_DATASET}
        VERBATIM)
endfunction()

function(mrtk_dataset_setup_test test name dest)
    get_property(_sha GLOBAL PROPERTY MRTK_DATASET_SHA256_${name})
    if(NOT _sha)
        message(FATAL_ERROR "dataset ${name} is not registered in tests/data/datasets.cmake")
    endif()
    add_test(NAME ${test}
        COMMAND ${CMAKE_COMMAND}
            -DDATASET=${name} -DSHA256=${_sha}
            -DURL_BASE=${MRTK_TEST_DATA_URL_BASE}
            "-DCACHE_DIR=${MRTK_TEST_DATA_CACHE}" "-DLOCAL_DIR=${MRTK_TEST_DATA_DIR}"
            -DDEST=${dest}
            -P ${_MRTK_FETCH_DATASET})
endfunction()

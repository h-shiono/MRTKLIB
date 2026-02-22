cmake_minimum_required(VERSION 3.15)
project(QZSSLIB VERSION 1.2.0 LANGUAGES C)

# Expose POSIX/BSD APIs hidden by -ansi (matches makefile platform detection)
if(APPLE)
    add_compile_definitions(_DARWIN_C_SOURCE)
elseif(UNIX)
    add_compile_definitions(_GNU_SOURCE)
endif()

# Compile options matching existing makefiles
add_compile_options(-Wall -O3 -ansi -pedantic -g)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-unused-but-set-variable)
endif()

# ── MRTKLIB modular library (new, C11) ───────────────────────────────────────
# New modular implementation that will gradually replace rtklib.
# Uses C11 (ISO/IEC 9899:2011) and the include/mrtklib/ public headers.
add_library(mrtklib STATIC
    src/mrtk_core.c
    src/mrtk_time.c
    src/mrtk_mat.c
    src/mrtk_coords.c
    src/mrtk_atmos.c
    src/mrtk_eph.c
    src/mrtk_peph.c
    src/mrtk_bits.c
    src/mrtk_sys.c
    src/mrtk_astro.c
    src/mrtk_antenna.c
    src/mrtk_station.c
    src/mrtk_obs.c
    src/mrtk_nav.c
    src/mrtk_rinex.c
    src/mrtk_tides.c
    src/mrtk_geoid.c
    src/mrtk_sbas.c
    src/mrtk_rtcm.c
    src/mrtk_rtcm2.c
    src/mrtk_rtcm3.c
    src/mrtk_rtcm3e.c
    src/mrtk_rtcm3lcl.c
    src/mrtk_ionex.c
    src/mrtk_opt.c
    src/mrtk_sol.c
    src/mrtk_bias_sinex.c
    src/mrtk_fcb.c
    src/mrtk_spp.c
    src/mrtk_madoca_local_corr.c
    src/mrtk_madoca_local_comb.c
    src/mrtk_madoca.c
    src/mrtk_lambda.c
)
target_include_directories(mrtklib PUBLIC include)
set_target_properties(mrtklib PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
# Override the global -ansi (= -std=c90) for this target: -std=c11 wins when placed after.
target_compile_options(mrtklib PRIVATE -std=c11)
target_compile_definitions(mrtklib PUBLIC
    ENAGLO ENAGAL ENAQZS ENACMP ENAIRN NFREQ=5 NEXOBS=5
)
target_link_libraries(mrtklib PUBLIC m)

# ── Core static library (legacy RTKLIB, C89/-ansi) ────────────────────────────
# Contains the union of all src/ files used by rnx2rtkp and rtkrcv.
# The linker pulls only the object files referenced by each executable.
add_library(rtklib STATIC
    src/options.c
    src/ppp.c           src/ppp_ar.c
    src/rcvraw.c
    src/rtkcmn.c        src/rtkpos.c        src/rtksvr.c
    src/stream.c
    src/rcv/binex.c     src/rcv/crescent.c  src/rcv/javad.c     src/rcv/novatel.c
    src/rcv/nvs.c       src/rcv/rt17.c      src/rcv/septentrio.c src/rcv/skytraq.c
    src/rcv/ss2.c       src/rcv/ublox.c
)
target_include_directories(rtklib PUBLIC src)
target_compile_definitions(rtklib PUBLIC TRACE)
target_link_libraries(rtklib PUBLIC m mrtklib)
if(UNIX AND NOT APPLE)
    target_link_libraries(rtklib PUBLIC rt)
endif()

# ── rnx2rtkp ─────────────────────────────────────────────────────────────────
# postpos.c uses PROGNAME so it must be compiled as part of this target
add_executable(rnx2rtkp
    apps/rnx2rtkp/rnx2rtkp.c
    src/postpos.c
)
target_compile_definitions(rnx2rtkp PRIVATE PROGNAME=\"rnx2rtkp\")
target_link_libraries(rnx2rtkp PRIVATE rtklib)

# ── rtkrcv ───────────────────────────────────────────────────────────────────
add_executable(rtkrcv
    apps/rtkrcv/rtkrcv.c
    apps/rtkrcv/vt.c
)
target_include_directories(rtkrcv PRIVATE apps/rtkrcv)
target_compile_definitions(rtkrcv PRIVATE PROGNAME=\"rtkrcv\" SVR_REUSEADDR)
target_link_libraries(rtkrcv PRIVATE rtklib pthread)

# ── recvbias ──────────────────────────────────────────────────────────────────
add_executable(recvbias
    apps/recvbias/recvbias.c
)
target_compile_definitions(recvbias PRIVATE PROGNAME=\"recvbias\")
target_link_libraries(recvbias PRIVATE rtklib)

# ── Install ───────────────────────────────────────────────────────────────────
# cmake --install build  →  copies binaries to bin/ (matches make install)
install(TARGETS rnx2rtkp rtkrcv recvbias RUNTIME DESTINATION bin)

# ── Regression tests ──────────────────────────────────────────────────────────
# Run with: ctest --test-dir build --output-on-failure
# Skip slow rtkrcv test: ctest --test-dir build -LE realtime
enable_testing()

set(_TESTDATA  ${CMAKE_SOURCE_DIR}/data)
set(_REFDATA   ${CMAKE_SOURCE_DIR}/tests/data/regression)
set(_TESTCMAKE ${CMAKE_SOURCE_DIR}/tests/cmake)

# ── Fixture: extract / clean test data archive ───────────────────────────────
add_test(NAME regression_setup
    COMMAND ${CMAKE_COMMAND} -E tar xzf ${_TESTDATA}/MALIB_OSS_data.tar.gz
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}   # archive has ./data/ prefix → extract from project root
)
set_tests_properties(regression_setup PROPERTIES
    FIXTURES_SETUP  regression_data
    TIMEOUT         30
)

# ── Fixture: download IONEX TEC file (for recvbias) ──────────────────────────
add_test(NAME regression_download_tec
    COMMAND ${CMAKE_COMMAND}
        -DTESTDATA_DIR=${_TESTDATA}
        -P ${_TESTCMAKE}/download_tec.cmake
)
set_tests_properties(regression_download_tec PROPERTIES
    FIXTURES_SETUP  regression_data
    TIMEOUT         60
)

add_test(NAME regression_cleanup
    COMMAND ${CMAKE_COMMAND}
        -DTESTDATA_DIR=${_TESTDATA}
        -P ${_TESTCMAKE}/cleanup_testdata.cmake
)
set_tests_properties(regression_cleanup PROPERTIES
    FIXTURES_CLEANUP regression_data
)

# ── rnx2rtkp PPP regression ───────────────────────────────────────────────────
add_test(NAME rnx2rtkp_ppp
    COMMAND $<TARGET_FILE:rnx2rtkp>
        -k ${CMAKE_SOURCE_DIR}/bin/rnx2rtkp.conf
        -o ${CMAKE_BINARY_DIR}/out_ppp.pos
        ${_TESTDATA}/MALIB_OSS_data_obsnav_240822-1100.obs
        ${_TESTDATA}/MALIB_OSS_data_obsnav_240822-1100.nav
        ${_TESTDATA}/2024235L.209.l6
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(rnx2rtkp_ppp PROPERTIES
    FIXTURES_REQUIRED regression_data
    TIMEOUT           180
    LABELS            "regression;ppp"
)

add_test(NAME rnx2rtkp_ppp_check
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT=${CMAKE_BINARY_DIR}/out_ppp.pos
        -DREFERENCE=${_REFDATA}/MALIB_OSS_data_obsnav_240822-1100.pp.pos
        -DTHRESHOLD=90
        -P ${_TESTCMAKE}/compare_pos.cmake
)
set_tests_properties(rnx2rtkp_ppp_check PROPERTIES
    DEPENDS           rnx2rtkp_ppp
    FIXTURES_REQUIRED regression_data
    LABELS            "regression;ppp"
)

# ── rnx2rtkp SPP regression ───────────────────────────────────────────────────
add_test(NAME rnx2rtkp_spp
    COMMAND $<TARGET_FILE:rnx2rtkp>
        -p 0 -sys G,R,E,J
        -o ${CMAKE_BINARY_DIR}/out_spp.pos
        ${_TESTDATA}/MALIB_OSS_data_obsnav_240822-1100.obs
        ${_TESTDATA}/MALIB_OSS_data_obsnav_240822-1100.nav
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(rnx2rtkp_spp PROPERTIES
    FIXTURES_REQUIRED regression_data
    TIMEOUT           60
    LABELS            "regression;spp"
)

add_test(NAME rnx2rtkp_spp_check
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT=${CMAKE_BINARY_DIR}/out_spp.pos
        -DREFERENCE=${_REFDATA}/MALIB_OSS_data_obsnav_240822-1100.pp.spp.pos
        -DTHRESHOLD=90
        -P ${_TESTCMAKE}/compare_pos.cmake
)
set_tests_properties(rnx2rtkp_spp_check PROPERTIES
    DEPENDS           rnx2rtkp_spp
    FIXTURES_REQUIRED regression_data
    LABELS            "regression;spp"
)

# ── recvbias regression ──────────────────────────────────────────────────────
# Note: recvbias returns exit code 1 on success (non-standard).
# The bash wrapper normalises this so CTest sees exit code 0.
add_test(NAME recvbias_rb
    COMMAND bash -c "\"$@\"; rc=$?; [ $rc -eq 1 ] && exit 0 || exit $rc" _
        $<TARGET_FILE:recvbias>
        -td 2024/8/22
        -nav ${_TESTDATA}/MALIB_OSS_data_obsnav_240822-1100.nav
        -tec ${_TESTDATA}/COD0OPSFIN_20242350000_01D_01H_GIM.INX
        -scb ${_TESTDATA}/2024235L.209.l6
        -pos -3961440.433,3308948.812,3734425.963
        -el 30
        -out ${CMAKE_BINARY_DIR}/out_rb.bia
        ${_TESTDATA}/MALIB_OSS_data_obsnav_240822-1100.obs
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(recvbias_rb PROPERTIES
    FIXTURES_REQUIRED regression_data
    TIMEOUT           120
    LABELS            "regression;recvbias"
)

add_test(NAME recvbias_rb_check
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT=${CMAKE_BINARY_DIR}/out_rb.bia
        -DREFERENCE=${_REFDATA}/MALIB_OSS_data_obsnav_240822-1100.rb.bia
        -DTHRESHOLD=90
        -P ${_TESTCMAKE}/compare_bia.cmake
)
set_tests_properties(recvbias_rb_check PROPERTIES
    DEPENDS           recvbias_rb
    FIXTURES_REQUIRED regression_data
    LABELS            "regression;recvbias"
)

# ── rtkrcv RT-PPP regression ──────────────────────────────────────────────────
# Slower test (~60-120s): replays SBF files at x10 speed via rtkrcv file stream.
# Uses a line-count threshold (≥90% of reference) rather than exact comparison
# because real-time cycle timing can vary slightly between runs.
add_test(NAME rtkrcv_rt
    COMMAND bash ${_TESTCMAKE}/run_rtkrcv_test.sh
        $<TARGET_FILE:rtkrcv>
        ${CMAKE_SOURCE_DIR}
        ${_REFDATA}/MALIB_OSS_data_obsnav_240822-1100.rt.pos
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(rtkrcv_rt PROPERTIES
    FIXTURES_REQUIRED regression_data
    TIMEOUT           300
    LABELS            "regression;realtime"
)

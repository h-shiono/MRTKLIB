cmake_minimum_required(VERSION 3.15)
project(MRTKLIB VERSION 0.1.0 LANGUAGES C)

# ── Version header generation ─────────────────────────────────────────────────
set(MRTKLIB_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(MRTKLIB_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(MRTKLIB_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(MRTKLIB_VERSION_STRING ${PROJECT_VERSION})
configure_file(
    ${CMAKE_SOURCE_DIR}/include/mrtklib/mrtk_version.h.in
    ${CMAKE_BINARY_DIR}/include/mrtklib/mrtk_version.h
    @ONLY
)

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
    # ── core/ (基盤・数学・ユーティリティ) ─────────────────────────────────────
    src/core/mrtk_context.c
    src/core/mrtk_core.c
    src/core/mrtk_time.c
    src/core/mrtk_mat.c
    src/core/mrtk_coords.c
    src/core/mrtk_bits.c
    src/core/mrtk_sys.c
    src/core/mrtk_trace.c
    # ── models/ (物理・環境モデル) ─────────────────────────────────────────────
    src/models/mrtk_atmos.c
    src/models/mrtk_astro.c
    src/models/mrtk_antenna.c
    src/models/mrtk_geoid.c
    src/models/mrtk_tides.c
    src/models/mrtk_station.c
    # ── data/ (データ構造・I/Oパース) ──────────────────────────────────────────
    src/data/mrtk_eph.c
    src/data/mrtk_peph.c
    src/data/mrtk_obs.c
    src/data/mrtk_nav.c
    src/data/mrtk_rinex.c
    src/data/mrtk_ionex.c
    src/data/mrtk_bias_sinex.c
    src/data/mrtk_fcb.c
    src/data/mrtk_sbas.c
    src/data/mrtk_rcvraw.c
    src/data/rcv/mrtk_rcv_binex.c
    src/data/rcv/mrtk_rcv_crescent.c
    src/data/rcv/mrtk_rcv_javad.c
    src/data/rcv/mrtk_rcv_novatel.c
    src/data/rcv/mrtk_rcv_nvs.c
    src/data/rcv/mrtk_rcv_rt17.c
    src/data/rcv/mrtk_rcv_septentrio.c
    src/data/rcv/mrtk_rcv_skytraq.c
    src/data/rcv/mrtk_rcv_ss2.c
    src/data/rcv/mrtk_rcv_ublox.c
    # ── rtcm/ (RTCM関連) ──────────────────────────────────────────────────────
    src/rtcm/mrtk_rtcm.c
    src/rtcm/mrtk_rtcm2.c
    src/rtcm/mrtk_rtcm3.c
    src/rtcm/mrtk_rtcm3e.c
    src/rtcm/mrtk_rtcm3lcl.c
    # ── madoca/ (MADOCA-PPP固有処理) ───────────────────────────────────────────
    src/madoca/mrtk_madoca.c
    src/madoca/mrtk_madoca_local_corr.c
    src/madoca/mrtk_madoca_local_comb.c
    # ── pos/ (測位アルゴリズム・オプション) ────────────────────────────────────
    src/pos/mrtk_spp.c
    src/pos/mrtk_rtkpos.c
    src/pos/mrtk_ppp_ar.c
    src/pos/mrtk_ppp.c
    src/pos/mrtk_lambda.c
    src/pos/mrtk_postpos.c
    src/pos/mrtk_sol.c
    src/pos/mrtk_opt.c
    src/pos/mrtk_options.c
    # ── stream/ (リアルタイム基盤) ─────────────────────────────────────────────
    src/stream/mrtk_stream.c
    src/stream/mrtk_rtksvr.c
)
target_include_directories(mrtklib PUBLIC include ${CMAKE_BINARY_DIR}/include)
set_target_properties(mrtklib PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
# Override the global -ansi (= -std=c90) for this target: -std=c11 wins when placed after.
target_compile_options(mrtklib PRIVATE -std=c11)
target_compile_definitions(mrtklib PUBLIC
    ENAGLO ENAGAL ENAQZS ENACMP ENAIRN NFREQ=5 NEXOBS=5 TRACE
)
target_link_libraries(mrtklib PUBLIC m)
if(UNIX AND NOT APPLE)
    target_link_libraries(mrtklib PUBLIC rt)
endif()

# ── System LAPACK/BLAS integration for fast matrix operations ────────────────
find_package(LAPACK)
if(LAPACK_FOUND)
    target_compile_definitions(mrtklib PRIVATE LAPACK)
    target_link_libraries(mrtklib PRIVATE LAPACK::LAPACK)
    message(STATUS "LAPACK found and enabled for fast matrix operations.")
else()
    message(WARNING "LAPACK not found. Falling back to internal (slower) matrix math.")
endif()

# ── rnx2rtkp ─────────────────────────────────────────────────────────────────
add_executable(rnx2rtkp
    apps/rnx2rtkp/rnx2rtkp.c
)
target_link_libraries(rnx2rtkp PRIVATE mrtklib)

# ── rtkrcv ───────────────────────────────────────────────────────────────────
add_executable(rtkrcv
    apps/rtkrcv/rtkrcv.c
    apps/rtkrcv/vt.c
)
target_include_directories(rtkrcv PRIVATE apps/rtkrcv)
target_compile_definitions(rtkrcv PRIVATE PROGNAME=\"rtkrcv\" SVR_REUSEADDR)
target_link_libraries(rtkrcv PRIVATE mrtklib pthread)

# ── recvbias ──────────────────────────────────────────────────────────────────
add_executable(recvbias
    apps/recvbias/recvbias.c
)
target_compile_definitions(recvbias PRIVATE PROGNAME=\"recvbias\")
target_link_libraries(recvbias PRIVATE mrtklib)

# ── Install ───────────────────────────────────────────────────────────────────
# cmake --install build  →  copies binaries to bin/ (matches make install)
install(TARGETS rnx2rtkp rtkrcv recvbias RUNTIME DESTINATION bin)

# ── Utility tools (optional) ─────────────────────────────────────────────────
# Build with: cmake --preset default -DBUILD_UTILS=ON
option(BUILD_UTILS "Build utility tools" OFF)
if(BUILD_UTILS)
    # Tools that depend on mrtklib
    add_executable(mrtk_rnx2rtcm  util/rnx2rtcm/rnx2rtcm.c)
    # Excluded (depend on removed/changed internal types):
    #   diffeph, dumplex, convlex, dumpssr, outlexion — lex_t removed
    #   geniono, genstec, estiono, gengrid, rcvdcb    — sstat_t/lam changed
    #   simobs — lam/nav_t.ion/satpos signature changed
    #   margelog — unfinished draft code with many bugs

    set(_UTIL_TARGETS
        mrtk_rnx2rtcm
    )
    foreach(tgt IN LISTS _UTIL_TARGETS)
        target_link_libraries(${tgt} PRIVATE mrtklib)
    endforeach()

    # Standalone tools excluded: gencrc, genmsk, genxor
    #   — need stdint.h but global -ansi flag prevents it
endif()

# ── Doxygen documentation ────────────────────────────────────────────────────
# Build with: cmake --build build --target doc
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(
        ${CMAKE_SOURCE_DIR}/Doxyfile.in
        ${CMAKE_BINARY_DIR}/Doxyfile
        @ONLY
    )
    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()

# ── Regression tests ──────────────────────────────────────────────────────────
# Run with: ctest --test-dir build --output-on-failure
# Skip slow rtkrcv test: ctest --test-dir build -LE realtime
enable_testing()

# ── Unit tests ───────────────────────────────────────────────────────────────
set(_UTEST_DIR ${CMAKE_SOURCE_DIR}/tests/utest)
set(_UTEST_SOURCES
    t_atmos t_coord t_geoid t_gloeph t_ionex
    t_lambda t_matrix t_misc t_ppp t_preceph
    t_rinex t_time
    # t_tle excluded: tle_pos() implementation not yet in library
)
foreach(test IN LISTS _UTEST_SOURCES)
    add_executable(${test} ${_UTEST_DIR}/${test}.c)
    target_link_libraries(${test} PRIVATE mrtklib)
    add_test(NAME utest_${test} COMMAND ${test}
             WORKING_DIRECTORY ${_UTEST_DIR})
    set_tests_properties(utest_${test} PROPERTIES LABELS "utest")
endforeach()

# ── Regression test data ─────────────────────────────────────────────────────
set(_TESTDATA  ${CMAKE_SOURCE_DIR}/tests/data/malib)
set(_REFDATA   ${CMAKE_SOURCE_DIR}/tests/data/malib)
set(_TESTCMAKE ${CMAKE_SOURCE_DIR}/tests/cmake)

# ── Fixture: extract / clean test data archive ───────────────────────────────
# Archive has ./data/ prefix internally; --strip-components=2 removes it.
add_test(NAME regression_setup
    COMMAND bash -c "tar xzf ${_TESTDATA}/MALIB_OSS_data.tar.gz --strip-components=2 -C ${_TESTDATA}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
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
        -k ${CMAKE_SOURCE_DIR}/conf/malib/rnx2rtkp.conf
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

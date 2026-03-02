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
    src/rtcm/mrtk_rtcm3_local_corr.c
    # ── madoca/ (MADOCA-PPP固有処理) ───────────────────────────────────────────
    src/madoca/mrtk_madoca.c
    src/madoca/mrtk_madoca_iono.c
    src/madoca/mrtk_madoca_local_corr.c
    src/madoca/mrtk_madoca_local_comb.c
    # ── clas/ (CLAS CSSR処理) ──────────────────────────────────────────────────
    src/clas/mrtk_clas.c
    src/clas/mrtk_clas_grid.c
    src/clas/mrtk_clas_osr.c

    # ── pos/ (測位アルゴリズム・オプション) ────────────────────────────────────
    src/pos/mrtk_spp.c
    src/pos/mrtk_rtkpos.c
    src/pos/mrtk_ppp_ar.c
    src/pos/mrtk_ppp.c
    src/pos/mrtk_ppp_rtk.c
    src/pos/mrtk_ppp_iono.c
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

# ── Python-based comparison tool ─────────────────────────────────────────────
find_package(Python3 COMPONENTS Interpreter)
set(_COMPARE_POS ${CMAKE_SOURCE_DIR}/scripts/tests/compare_pos.py)

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
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_POS}
        ${_REFDATA}/MALIB_OSS_data_obsnav_240822-1100.pp.spp.pos
        ${CMAKE_BINARY_DIR}/out_spp.pos
        --tolerance 0.005
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

# ═══════════════════════════════════════════════════════════════════════════════
# MADOCALIB regression tests
# ═══════════════════════════════════════════════════════════════════════════════
# Data: MIZU station, 2025/04/01 Session A (00:00-00:59)
# Source: upstream/madocalib/sample_data
# Test data archive: tests/data/madocalib/madocalib_testdata.tar.gz
#
# Tests:
#   madocalib_ppp         PPP (sample.conf)           L6E mdc-004
#   madocalib_pppar       PPP-AR (sample_pppar.conf)  L6E mdc-004
#   madocalib_pppar_003   PPP-AR (sample_pppar.conf)  L6E mdc-003
#   madocalib_pppar_ion   PPP-AR+iono (sample_pppar_iono.conf) L6E+L6D mdc-004
#   madocalib_cssr2ssr    CSSR→SSR conversion (cssr2ssr, not yet ported)
set(_MDLDATA  ${CMAKE_SOURCE_DIR}/tests/data/madocalib)

# ── Fixture: extract / clean madocalib test data archive ─────────────────────
add_test(NAME madocalib_setup
    COMMAND bash -c "tar xzf ${_MDLDATA}/madocalib_testdata.tar.gz -C ${_MDLDATA}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(madocalib_setup PROPERTIES
    FIXTURES_SETUP  madocalib_data
    TIMEOUT         30
    LABELS          "regression;madocalib"
)

add_test(NAME madocalib_cleanup
    COMMAND bash -c "rm -f \
        ${_MDLDATA}/MIZU00JPN_R_20250910000_01D_30S_MO.obs \
        ${_MDLDATA}/BRDM00DLR_S_20250910000_01D_MN.rnx \
        ${_MDLDATA}/2025091A.204.l6 \
        ${_MDLDATA}/2025091A.206.l6 \
        ${_MDLDATA}/2025091A.204.mdc003.l6 \
        ${_MDLDATA}/2025091A.206.mdc003.l6 \
        ${_MDLDATA}/2025091A.200.l6 \
        ${_MDLDATA}/2025091A.201.l6 \
        ${_MDLDATA}/igs20.atx"
)
set_tests_properties(madocalib_cleanup PROPERTIES
    FIXTURES_CLEANUP madocalib_data
    LABELS           "regression;madocalib"
)

# ── rnx2rtkp MADOCA-PPP regression ──────────────────────────────────────────
add_test(NAME madocalib_ppp
    COMMAND $<TARGET_FILE:rnx2rtkp>
        -k ${CMAKE_SOURCE_DIR}/conf/madocalib/rnx2rtkp.conf
        -ts 2025/04/01 0:00:00 -te 2025/04/01 0:59:30 -ti 30
        -o ${CMAKE_BINARY_DIR}/out_madocalib_ppp.pos
        ${_MDLDATA}/MIZU00JPN_R_20250910000_01D_30S_MO.obs
        ${_MDLDATA}/BRDM00DLR_S_20250910000_01D_MN.rnx
        ${_MDLDATA}/2025091A.204.l6
        ${_MDLDATA}/2025091A.206.l6
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(madocalib_ppp PROPERTIES
    FIXTURES_REQUIRED madocalib_data
    TIMEOUT           180
    LABELS            "regression;madocalib;ppp"
)

add_test(NAME madocalib_ppp_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_POS}
        ${_MDLDATA}/madocalib_MIZU_20250401-0000.pp.pos
        ${CMAKE_BINARY_DIR}/out_madocalib_ppp.pos
        --tolerance 0.005
)
set_tests_properties(madocalib_ppp_check PROPERTIES
    DEPENDS           madocalib_ppp
    FIXTURES_REQUIRED madocalib_data
    LABELS            "regression;madocalib;ppp"
)

# ── rnx2rtkp MADOCA PPP-AR regression (mdc-004) ─────────────────────────────
add_test(NAME madocalib_pppar
    COMMAND $<TARGET_FILE:rnx2rtkp>
        -k ${CMAKE_SOURCE_DIR}/conf/madocalib/rnx2rtkp_pppar.conf
        -ts 2025/04/01 0:00:00 -te 2025/04/01 0:59:30 -ti 30
        -o ${CMAKE_BINARY_DIR}/out_madocalib_pppar.pos
        ${_MDLDATA}/MIZU00JPN_R_20250910000_01D_30S_MO.obs
        ${_MDLDATA}/BRDM00DLR_S_20250910000_01D_MN.rnx
        ${_MDLDATA}/2025091A.204.l6
        ${_MDLDATA}/2025091A.206.l6
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(madocalib_pppar PROPERTIES
    FIXTURES_REQUIRED madocalib_data
    TIMEOUT           180
    LABELS            "regression;madocalib;pppar"
)

add_test(NAME madocalib_pppar_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_POS}
        ${_MDLDATA}/madocalib_MIZU_20250401-0000.pppar.pos
        ${CMAKE_BINARY_DIR}/out_madocalib_pppar.pos
        --tolerance 0.02
)
set_tests_properties(madocalib_pppar_check PROPERTIES
    DEPENDS           madocalib_pppar
    FIXTURES_REQUIRED madocalib_data
    LABELS            "regression;madocalib;pppar"
)

# ── rnx2rtkp MADOCA PPP-AR regression (mdc-003) ─────────────────────────────
add_test(NAME madocalib_pppar_003
    COMMAND $<TARGET_FILE:rnx2rtkp>
        -k ${CMAKE_SOURCE_DIR}/conf/madocalib/rnx2rtkp_pppar.conf
        -ts 2025/04/01 0:00:00 -te 2025/04/01 0:59:30 -ti 30
        -o ${CMAKE_BINARY_DIR}/out_madocalib_pppar_003.pos
        ${_MDLDATA}/MIZU00JPN_R_20250910000_01D_30S_MO.obs
        ${_MDLDATA}/BRDM00DLR_S_20250910000_01D_MN.rnx
        ${_MDLDATA}/2025091A.204.mdc003.l6
        ${_MDLDATA}/2025091A.206.mdc003.l6
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(madocalib_pppar_003 PROPERTIES
    FIXTURES_REQUIRED madocalib_data
    TIMEOUT           180
    LABELS            "regression;madocalib;pppar"
)

add_test(NAME madocalib_pppar_003_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_POS}
        ${_MDLDATA}/madocalib_MIZU_20250401-0000.pppar_003.pos
        ${CMAKE_BINARY_DIR}/out_madocalib_pppar_003.pos
        --tolerance 0.005
)
set_tests_properties(madocalib_pppar_003_check PROPERTIES
    DEPENDS           madocalib_pppar_003
    FIXTURES_REQUIRED madocalib_data
    LABELS            "regression;madocalib;pppar"
)

# ── rnx2rtkp MADOCA PPP-AR + ionosphere regression ──────────────────────────
# Uses L6E (.204/.206) + L6D (.200/.201) for ionospheric correction
add_test(NAME madocalib_pppar_ion
    COMMAND $<TARGET_FILE:rnx2rtkp>
        -k ${CMAKE_SOURCE_DIR}/conf/madocalib/rnx2rtkp_pppar_iono.conf
        -ts 2025/04/01 0:00:00 -te 2025/04/01 0:59:30 -ti 30
        -o ${CMAKE_BINARY_DIR}/out_madocalib_pppar_ion.pos
        ${_MDLDATA}/MIZU00JPN_R_20250910000_01D_30S_MO.obs
        ${_MDLDATA}/BRDM00DLR_S_20250910000_01D_MN.rnx
        ${_MDLDATA}/2025091A.204.l6
        ${_MDLDATA}/2025091A.206.l6
        ${_MDLDATA}/2025091A.200.l6
        ${_MDLDATA}/2025091A.201.l6
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(madocalib_pppar_ion PROPERTIES
    FIXTURES_REQUIRED madocalib_data
    TIMEOUT           180
    LABELS            "regression;madocalib;pppar"
)

add_test(NAME madocalib_pppar_ion_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_POS}
        ${_MDLDATA}/madocalib_MIZU_20250401-0000.pppar_ion.pos
        ${CMAKE_BINARY_DIR}/out_madocalib_pppar_ion.pos
        --tolerance 0.04
)
set_tests_properties(madocalib_pppar_ion_check PROPERTIES
    DEPENDS           madocalib_pppar_ion
    FIXTURES_REQUIRED madocalib_data
    LABELS            "regression;madocalib;pppar"
)

# ── cssr2ssr regression (not yet ported) ─────────────────────────────────────
# TODO: cssr2ssr binary is not yet integrated into MRTKLIB.
#       When ported, add:
#   add_test(NAME madocalib_cssr2ssr
#       COMMAND $<TARGET_FILE:cssr2ssr>
#           ${_MDLDATA}/2025091A.204.l6
#           -o ${CMAKE_BINARY_DIR}/out_cssr2ssr.rtcm3
#           -td 2025/04/01 -d
#       WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
#   )

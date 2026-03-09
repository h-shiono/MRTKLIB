cmake_minimum_required(VERSION 3.15)
project(MRTKLIB VERSION 0.4.1 LANGUAGES C)

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
    src/clas/mrtk_clas_isb.c

    # ── pos/ (測位アルゴリズム・オプション) ────────────────────────────────────
    src/pos/mrtk_spp.c
    src/pos/mrtk_rtkpos.c
    src/pos/mrtk_ppp_ar.c
    src/pos/mrtk_ppp.c
    src/pos/mrtk_ppp_rtk.c
    src/pos/mrtk_vrs.c
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

# ── ssr2obs ──────────────────────────────────────────────────────────────────
add_executable(ssr2obs
    apps/ssr2obs/ssr2obs.c
)
target_link_libraries(ssr2obs PRIVATE mrtklib)

# ── ssr2osr ──────────────────────────────────────────────────────────────────
add_executable(ssr2osr
    apps/ssr2osr/ssr2osr.c
)
target_link_libraries(ssr2osr PRIVATE mrtklib)

# ── dumpcssr ─────────────────────────────────────────────────────────────────
add_executable(dumpcssr
    apps/dumpcssr/dumpcssr.c
)
target_link_libraries(dumpcssr PRIVATE mrtklib)

# ── Install ───────────────────────────────────────────────────────────────────
# cmake --install build  →  copies binaries to bin/ (matches make install)
install(TARGETS rnx2rtkp rtkrcv recvbias ssr2obs ssr2osr dumpcssr RUNTIME DESTINATION bin)

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
set(_COMPARE_POS      ${CMAKE_SOURCE_DIR}/scripts/tests/compare_pos.py)
set(_COMPARE_POS_ABS  ${CMAKE_SOURCE_DIR}/scripts/tests/compare_pos_abs.py)
set(_COMPARE_NMEA_ABS ${CMAKE_SOURCE_DIR}/scripts/tests/compare_nmea_abs.py)
set(_CHECK_SCATTER    ${CMAKE_SOURCE_DIR}/scripts/tests/check_pos_scatter.py)

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
    RESOURCE_LOCK     rtkrcv_port
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

# The reference .pos files were generated with upstream MADOCALIB built with LAPACK.
# Without LAPACK, MRTKLIB falls back to the internal LU solver (same as upstream default),
# which diverges ~1.5–3.8 cm from the LAPACK reference. Use wider tolerances in that case.
if(LAPACK_FOUND)
    set(_PPPAR_TOL     0.008)
    set(_PPPAR_ION_TOL 0.005)
else()
    set(_PPPAR_TOL     0.020)
    set(_PPPAR_ION_TOL 0.040)
endif()

add_test(NAME madocalib_pppar_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_POS}
        ${_MDLDATA}/madocalib_MIZU_20250401-0000.pppar.pos
        ${CMAKE_BINARY_DIR}/out_madocalib_pppar.pos
        --tolerance ${_PPPAR_TOL}
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
        --tolerance ${_PPPAR_ION_TOL}
)
set_tests_properties(madocalib_pppar_ion_check PROPERTIES
    DEPENDS           madocalib_pppar_ion
    FIXTURES_REQUIRED madocalib_data
    LABELS            "regression;madocalib;pppar"
)

# ── Absolute accuracy check: PPP-AR vs SINEX MIZU true coordinate ─────────────
# Reference: IGS weekly SINEX (GPS week 2383, 2025/09/08-09/14) for MIZU station.
# Evaluation: 2D horizontal 1σ and 95th-percentile against ITRF true coordinate.
#   --use-2d: PPP vertical accuracy is troposphere-model-limited (~20 cm typical);
#             2D horizontal (~4 cm) is the meaningful absolute accuracy metric.
# Pass: metric < tolerance  OR  metric < SINEX formal precision (0.54 mm).
# Convergence: skip first 60 epochs (30 min, 30-s interval) to discard transient.
set(_SINEX_MIZU ${_MDLDATA}/IGS0OPSSNX_20252500000_07D_07D_SOL.SNX.gz)
add_test(NAME madocalib_pppar_abs_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_POS_ABS}
        --sinex ${_SINEX_MIZU}
        --station MIZU
        --tolerance 0.100
        --skip-epochs 60
        --use-2d
        ${CMAKE_BINARY_DIR}/out_madocalib_pppar.pos
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(madocalib_pppar_abs_check PROPERTIES
    DEPENDS           madocalib_pppar
    FIXTURES_REQUIRED madocalib_data
    LABELS            "absolute;madocalib;pppar"
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

# ═══════════════════════════════════════════════════════════════════════════════
# CLASLIB PPP-RTK regression tests
# ═══════════════════════════════════════════════════════════════════════════════
# Data: 0627 station, 2019/08/27 Session Q (16:00-17:00) and 2019/12/15
# Source: upstream/claslib/data
# Test data archive: tests/data/claslib/claslib_testdata.tar.gz
#
# Tests (8/8 upstream claslib coverage):
#   claslib_ppp_rtk         PPP-RTK with L6 (RINEX obs, 2019/239)
#   claslib_ppp_rtk_bnx     PPP-RTK with L6 (BINEX obs, 2019/239)
#   claslib_ppp_rtk_2ch     PPP-RTK with 2-channel L6 (2025/157)
#   claslib_ppp_rtk_st12    PPP-RTK with SubType 12 (BINEX obs, 2019/349)
#   claslib_ppp_rtk_l1cl5   PPP-RTK with L1C+L5 frequency (2025/022)
#   claslib_vrs_rtk         VRS-RTK (2019/239)
#   claslib_vrs_rtk_st12    VRS-RTK with SubType 12 (2019/349)
#   claslib_vrs_rtk_2ch     VRS-RTK with 2-channel (2025/157)
#   rtkrcv_rt_clas          Real-time CLAS PPP-RTK (BINEX+L6, rtkrcv file replay)
set(_CLSDATA  ${CMAKE_SOURCE_DIR}/tests/data/claslib)
set(_COMPARE_NMEA ${CMAKE_SOURCE_DIR}/scripts/tests/compare_nmea.py)

# ── Fixture: extract / clean claslib test data archive ────────────────────────
add_test(NAME claslib_setup
    COMMAND bash -c "tar xzf ${_CLSDATA}/claslib_testdata.tar.gz -C ${_CLSDATA}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(claslib_setup PROPERTIES
    FIXTURES_SETUP  claslib_data
    TIMEOUT         60
    LABELS          "regression;claslib"
)

add_test(NAME claslib_cleanup
    COMMAND bash -c "rm -f \
        ${_CLSDATA}/0627239Q.obs \
        ${_CLSDATA}/0627239Q.bnx \
        ${_CLSDATA}/0627349AB.bnx \
        ${_CLSDATA}/0627157U.bnx \
        ${_CLSDATA}/0627022Q.bnx \
        ${_CLSDATA}/sept_2019239.nav \
        ${_CLSDATA}/sept_2019349.nav \
        ${_CLSDATA}/0627157U.nav \
        ${_CLSDATA}/2019239Q.l6 \
        ${_CLSDATA}/2019349B.l6 \
        ${_CLSDATA}/2025157U_ch1.l6 \
        ${_CLSDATA}/2025157U_ch2.l6 \
        ${_CLSDATA}/2025022Q.l6 \
        ${_CLSDATA}/clas_grid.def \
        ${_CLSDATA}/clas_grid.blq \
        ${_CLSDATA}/igu00p01.erp \
        ${_CLSDATA}/igs14_L5copy.atx \
        ${_CLSDATA}/isb.tbl \
        ${_CLSDATA}/l2csft.tbl \
        ${_CLSDATA}/vrs2019239Q.obs \
        ${_CLSDATA}/vrs2019349B.obs \
        ${_CLSDATA}/2025157U_ch1.obs \
        ${_CLSDATA}/2025157U_ch2.obs \
        ${_CLSDATA}/0627239Q.bnx.tag \
        ${_CLSDATA}/2019239Q.l6.tag \
        ${_CLSDATA}/rtkrcv_clas_ref.nmea"
)
set_tests_properties(claslib_cleanup PROPERTIES
    FIXTURES_CLEANUP claslib_data
    LABELS           "regression;claslib"
)

# ── rnx2rtkp CLAS PPP-RTK regression (L6, RINEX obs) ─────────────────────────
# Data: 2019/08/27 16:00-17:00, 0627239Q.obs + sept_2019239.nav + 2019239Q.l6
# Reference: ref_L6.nmea (upstream claslib output)
add_test(NAME claslib_ppp_rtk
    COMMAND $<TARGET_FILE:rnx2rtkp>
        -k ${CMAKE_SOURCE_DIR}/conf/claslib/rnx2rtkp.conf
        -ts 2019/08/27 16:00:00 -te 2019/08/27 16:59:59 -ti 1
        -o ${CMAKE_BINARY_DIR}/out_claslib_ppp_rtk.nmea
        ${_CLSDATA}/0627239Q.obs
        ${_CLSDATA}/sept_2019239.nav
        ${_CLSDATA}/2019239Q.l6
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(claslib_ppp_rtk PROPERTIES
    FIXTURES_REQUIRED claslib_data
    TIMEOUT           300
    LABELS            "regression;claslib;ppp-rtk"
)

add_test(NAME claslib_ppp_rtk_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_NMEA}
        ${_CLSDATA}/ref_L6.nmea
        ${CMAKE_BINARY_DIR}/out_claslib_ppp_rtk.nmea
        --tolerance 0.10
)
set_tests_properties(claslib_ppp_rtk_check PROPERTIES
    DEPENDS           claslib_ppp_rtk
    FIXTURES_REQUIRED claslib_data
    LABELS            "regression;claslib;ppp-rtk"
)

# ── rnx2rtkp CLAS PPP-RTK regression (L6, BINEX obs) ─────────────────────────
# Data: 2019/08/27 16:00-17:00, 0627239Q.bnx + sept_2019239.nav + 2019239Q.l6
# Reference: ref_L6_bnx.nmea (upstream claslib output)
add_test(NAME claslib_ppp_rtk_bnx
    COMMAND $<TARGET_FILE:rnx2rtkp>
        -k ${CMAKE_SOURCE_DIR}/conf/claslib/rnx2rtkp.conf
        -ts 2019/08/27 16:00:00 -te 2019/08/27 16:59:59 -ti 1
        -o ${CMAKE_BINARY_DIR}/out_claslib_ppp_rtk_bnx.nmea
        ${_CLSDATA}/0627239Q.bnx
        ${_CLSDATA}/sept_2019239.nav
        ${_CLSDATA}/2019239Q.l6
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(claslib_ppp_rtk_bnx PROPERTIES
    FIXTURES_REQUIRED claslib_data
    TIMEOUT           300
    LABELS            "regression;claslib;ppp-rtk"
)

add_test(NAME claslib_ppp_rtk_bnx_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_NMEA}
        ${_CLSDATA}/ref_L6_bnx.nmea
        ${CMAKE_BINARY_DIR}/out_claslib_ppp_rtk_bnx.nmea
        --tolerance 0.10
)
set_tests_properties(claslib_ppp_rtk_bnx_check PROPERTIES
    DEPENDS           claslib_ppp_rtk_bnx
    FIXTURES_REQUIRED claslib_data
    LABELS            "regression;claslib;ppp-rtk"
)

# ── rnx2rtkp CLAS PPP-RTK regression (dual-channel L6) ────────────────────────
# Data: 2025/06/06 20:00-21:00, 0627157U.bnx + 2025157U_ch1.l6 + ch2.l6
# Reference: ref_L6_2CH.nmea (upstream claslib output with -l6msg 1)
# Note: tolerance 0.20m due to wider accuracy gap on 2025 dataset (~15.6cm RMS)
add_test(NAME claslib_ppp_rtk_2ch
    COMMAND $<TARGET_FILE:rnx2rtkp>
        -k ${CMAKE_SOURCE_DIR}/conf/claslib/rnx2rtkp.conf
        -ts 2025/06/06 20:00:00 -te 2025/06/06 20:59:59 -ti 1
        -l6msg 1
        -o ${CMAKE_BINARY_DIR}/out_claslib_ppp_rtk_2ch.nmea
        ${_CLSDATA}/0627157U.bnx
        ${_CLSDATA}/0627157U.nav
        ${_CLSDATA}/2025157U_ch1.l6
        ${_CLSDATA}/2025157U_ch2.l6
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(claslib_ppp_rtk_2ch PROPERTIES
    FIXTURES_REQUIRED claslib_data
    TIMEOUT           300
    LABELS            "regression;claslib;ppp-rtk"
)

add_test(NAME claslib_ppp_rtk_2ch_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_NMEA}
        ${_CLSDATA}/ref_L6_2CH.nmea
        ${CMAKE_BINARY_DIR}/out_claslib_ppp_rtk_2ch.nmea
        --tolerance 0.20
        --skip-fixrate
)
set_tests_properties(claslib_ppp_rtk_2ch_check PROPERTIES
    DEPENDS           claslib_ppp_rtk_2ch
    FIXTURES_REQUIRED claslib_data
    LABELS            "regression;claslib;ppp-rtk"
)

# ── Absolute accuracy check: CLAS PPP-RTK 2CH vs GSI F5 TSUKUBA3 ─────────────
# Reference: GSI F5 station 960627 (TSUKUBA3/0627), 15-day median centred on
# 2025/06/06 (DOY 157).  Pass/fail on 2D horizontal accuracy (1σ, 95%).
# Tolerance 0.300 m: MRTKLIB 2CH achieves 88% fix rate (float epochs present),
# making the 95th percentile ~26 cm.  Tier-1 test already guards convergence;
# this Tier-2 test confirms geodetic plausibility at a generous threshold.
set(_F5_0627 ${_CLSDATA}/960627.25.pos)
add_test(NAME claslib_ppp_rtk_2ch_abs_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_NMEA_ABS}
        --f5 ${_F5_0627}
        --date 2025/06/06
        --tolerance 0.300
        ${CMAKE_BINARY_DIR}/out_claslib_ppp_rtk_2ch.nmea
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(claslib_ppp_rtk_2ch_abs_check PROPERTIES
    DEPENDS           claslib_ppp_rtk_2ch
    FIXTURES_REQUIRED claslib_data
    LABELS            "absolute;claslib;ppp-rtk"
)

# ── rnx2rtkp CLAS VRS-RTK regression ──────────────────────────────────────────
# Data: 2019/08/27 16:00-17:00, 0627239Q.obs + sept_2019239.nav + vrs2019239Q.obs
# Reference: ref_VRS.nmea (upstream claslib output)
add_test(NAME claslib_vrs_rtk
    COMMAND $<TARGET_FILE:rnx2rtkp>
        -k ${CMAKE_SOURCE_DIR}/conf/claslib/rnx2rtkp_vrs.conf
        -ts 2019/08/27 16:00:00 -te 2019/08/27 16:59:59 -ti 1
        -o ${CMAKE_BINARY_DIR}/out_claslib_vrs_rtk.nmea
        ${_CLSDATA}/0627239Q.obs
        ${_CLSDATA}/sept_2019239.nav
        ${_CLSDATA}/vrs2019239Q.obs
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(claslib_vrs_rtk PROPERTIES
    FIXTURES_REQUIRED claslib_data
    TIMEOUT           300
    LABELS            "regression;claslib;vrs"
)

add_test(NAME claslib_vrs_rtk_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_NMEA}
        ${_CLSDATA}/ref_VRS.nmea
        ${CMAKE_BINARY_DIR}/out_claslib_vrs_rtk.nmea
        --tolerance 0.10
)
set_tests_properties(claslib_vrs_rtk_check PROPERTIES
    DEPENDS           claslib_vrs_rtk
    FIXTURES_REQUIRED claslib_data
    LABELS            "regression;claslib;vrs"
)

# ── rnx2rtkp CLAS PPP-RTK regression (SubType 12 BINEX) ─────────────────────
# Data: 2019/12/15 01:00-02:00, 0627349AB.bnx + sept_2019349.nav + 2019349B.l6
# Reference: ref_ST12.nmea (upstream claslib output)
add_test(NAME claslib_ppp_rtk_st12
    COMMAND $<TARGET_FILE:rnx2rtkp>
        -k ${CMAKE_SOURCE_DIR}/conf/claslib/rnx2rtkp.conf
        -ts 2019/12/15 01:00:00 -te 2019/12/15 01:59:59 -ti 1
        -o ${CMAKE_BINARY_DIR}/out_claslib_ppp_rtk_st12.nmea
        ${_CLSDATA}/0627349AB.bnx
        ${_CLSDATA}/sept_2019349.nav
        ${_CLSDATA}/2019349B.l6
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(claslib_ppp_rtk_st12 PROPERTIES
    FIXTURES_REQUIRED claslib_data
    TIMEOUT           300
    LABELS            "regression;claslib;ppp-rtk"
)

add_test(NAME claslib_ppp_rtk_st12_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_NMEA}
        ${_CLSDATA}/ref_ST12.nmea
        ${CMAKE_BINARY_DIR}/out_claslib_ppp_rtk_st12.nmea
        --tolerance 0.15
)
set_tests_properties(claslib_ppp_rtk_st12_check PROPERTIES
    DEPENDS           claslib_ppp_rtk_st12
    FIXTURES_REQUIRED claslib_data
    LABELS            "regression;claslib;ppp-rtk"
)

# ── rnx2rtkp CLAS PPP-RTK regression (L1C+L5 frequency) ─────────────────────
# Data: 2025/01/22 16:00-17:00, 0627022Q.bnx + 2025022Q.l6 (no separate nav)
# Reference: ref_L6_L1CL5.nmea (upstream claslib output)
add_test(NAME claslib_ppp_rtk_l1cl5
    COMMAND $<TARGET_FILE:rnx2rtkp>
        -k ${CMAKE_SOURCE_DIR}/conf/claslib/rnx2rtkp_L1CL5.conf
        -ts 2025/01/22 16:00:00 -te 2025/01/22 16:59:59 -ti 1
        -o ${CMAKE_BINARY_DIR}/out_claslib_ppp_rtk_l1cl5.nmea
        ${_CLSDATA}/0627022Q.bnx
        ${_CLSDATA}/2025022Q.l6
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(claslib_ppp_rtk_l1cl5 PROPERTIES
    FIXTURES_REQUIRED claslib_data
    TIMEOUT           300
    LABELS            "regression;claslib;ppp-rtk"
)

add_test(NAME claslib_ppp_rtk_l1cl5_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_NMEA}
        ${_CLSDATA}/ref_L6_L1CL5.nmea
        ${CMAKE_BINARY_DIR}/out_claslib_ppp_rtk_l1cl5.nmea
        --tolerance 0.12
)
set_tests_properties(claslib_ppp_rtk_l1cl5_check PROPERTIES
    DEPENDS           claslib_ppp_rtk_l1cl5
    FIXTURES_REQUIRED claslib_data
    LABELS            "regression;claslib;ppp-rtk"
)

# ── rnx2rtkp CLAS VRS-RTK regression (SubType 12 BINEX) ─────────────────────
# Data: 2019/12/15 01:00-02:00, 0627349AB.bnx + sept_2019349.nav + vrs2019349B.obs
# Reference: ref_ST12_VRS.nmea (upstream claslib output)
add_test(NAME claslib_vrs_rtk_st12
    COMMAND $<TARGET_FILE:rnx2rtkp>
        -k ${CMAKE_SOURCE_DIR}/conf/claslib/rnx2rtkp_vrs.conf
        -ts 2019/12/15 01:00:00 -te 2019/12/15 01:59:59 -ti 1
        -o ${CMAKE_BINARY_DIR}/out_claslib_vrs_rtk_st12.nmea
        ${_CLSDATA}/0627349AB.bnx
        ${_CLSDATA}/sept_2019349.nav
        ${_CLSDATA}/vrs2019349B.obs
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(claslib_vrs_rtk_st12 PROPERTIES
    FIXTURES_REQUIRED claslib_data
    TIMEOUT           300
    LABELS            "regression;claslib;vrs"
)

add_test(NAME claslib_vrs_rtk_st12_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_NMEA}
        ${_CLSDATA}/ref_ST12_VRS.nmea
        ${CMAKE_BINARY_DIR}/out_claslib_vrs_rtk_st12.nmea
        --tolerance 0.10
)
set_tests_properties(claslib_vrs_rtk_st12_check PROPERTIES
    DEPENDS           claslib_vrs_rtk_st12
    FIXTURES_REQUIRED claslib_data
    LABELS            "regression;claslib;vrs"
)

# ── rnx2rtkp CLAS VRS-RTK regression (dual-channel) ─────────────────────────
# Data: 2025/06/06 20:00-21:00, 0627157U.bnx + ch1.obs + ch2.obs (VRS)
# Reference: ref_L6_2CH_VRS.nmea (upstream claslib output)
# Note: No separate .nav — nav embedded in BINEX; no .l6 — VRS obs replace L6
add_test(NAME claslib_vrs_rtk_2ch
    COMMAND $<TARGET_FILE:rnx2rtkp>
        -k ${CMAKE_SOURCE_DIR}/conf/claslib/rnx2rtkp_vrs.conf
        -ts 2025/06/06 20:00:00 -te 2025/06/06 20:59:59 -ti 1
        -l6msg 1
        -o ${CMAKE_BINARY_DIR}/out_claslib_vrs_rtk_2ch.nmea
        ${_CLSDATA}/0627157U.bnx
        ${_CLSDATA}/2025157U_ch1.obs
        ${_CLSDATA}/2025157U_ch2.obs
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(claslib_vrs_rtk_2ch PROPERTIES
    FIXTURES_REQUIRED claslib_data
    TIMEOUT           300
    LABELS            "regression;claslib;vrs"
)

add_test(NAME claslib_vrs_rtk_2ch_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_NMEA}
        ${_CLSDATA}/ref_L6_2CH_VRS.nmea
        ${CMAKE_BINARY_DIR}/out_claslib_vrs_rtk_2ch.nmea
        --tolerance 0.20
        --skip-fixrate
)
set_tests_properties(claslib_vrs_rtk_2ch_check PROPERTIES
    DEPENDS           claslib_vrs_rtk_2ch
    FIXTURES_REQUIRED claslib_data
    LABELS            "regression;claslib;vrs"
)

# ── ssr2obs regression: generate VRS and verify via VRS-RTK pipeline ─────────
# Step 1: Generate VRS observations from SSR corrections using ssr2obs
add_test(NAME claslib_ssr2obs
    COMMAND $<TARGET_FILE:ssr2obs>
        -k ${CMAKE_SOURCE_DIR}/conf/claslib/ssr2obs.conf
        -ts 2019/08/27 16:00:00 -te 2019/08/27 16:59:59 -ti 1
        -r -o ${CMAKE_BINARY_DIR}/out_ssr2obs_vrs2019239Q.obs
        ${_CLSDATA}/sept_2019239.nav ${_CLSDATA}/2019239Q.l6
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(claslib_ssr2obs PROPERTIES
    FIXTURES_REQUIRED claslib_data
    TIMEOUT           300
    LABELS            "regression;claslib;vrs"
)

# Step 2: Feed ssr2obs output into VRS-RTK and compare against reference
add_test(NAME claslib_ssr2obs_vrs_rtk
    COMMAND $<TARGET_FILE:rnx2rtkp>
        -k ${CMAKE_SOURCE_DIR}/conf/claslib/rnx2rtkp_vrs.conf
        -ts 2019/08/27 16:00:00 -te 2019/08/27 16:59:59 -ti 1
        -o ${CMAKE_BINARY_DIR}/out_ssr2obs_vrs_rtk.nmea
        ${_CLSDATA}/0627239Q.obs
        ${_CLSDATA}/sept_2019239.nav
        ${CMAKE_BINARY_DIR}/out_ssr2obs_vrs2019239Q.obs
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(claslib_ssr2obs_vrs_rtk PROPERTIES
    DEPENDS           claslib_ssr2obs
    FIXTURES_REQUIRED claslib_data
    TIMEOUT           300
    LABELS            "regression;claslib;vrs"
)

add_test(NAME claslib_ssr2obs_vrs_rtk_check
    COMMAND ${Python3_EXECUTABLE} ${_COMPARE_NMEA}
        ${_CLSDATA}/ref_VRS.nmea
        ${CMAKE_BINARY_DIR}/out_ssr2obs_vrs_rtk.nmea
        --tolerance 0.20
        --skip-fixrate
)
set_tests_properties(claslib_ssr2obs_vrs_rtk_check PROPERTIES
    DEPENDS           claslib_ssr2obs_vrs_rtk
    FIXTURES_REQUIRED claslib_data
    LABELS            "regression;claslib;vrs"
)

# ── ssr2osr regression: apply SSR corrections to real obs and verify output ──
# Step 1: Run ssr2osr with real observations + L6 corrections
add_test(NAME claslib_ssr2osr
    COMMAND $<TARGET_FILE:ssr2osr>
        -k ${CMAKE_SOURCE_DIR}/conf/claslib/ssr2osr.conf
        -ts 2019/08/27 16:00:00 -te 2019/08/27 16:59:59 -ti 1
        -o ${CMAKE_BINARY_DIR}/out_ssr2osr.nmea
        ${_CLSDATA}/0627239Q.obs ${_CLSDATA}/sept_2019239.nav ${_CLSDATA}/2019239Q.l6
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(claslib_ssr2osr PROPERTIES
    FIXTURES_REQUIRED claslib_data
    TIMEOUT           300
    LABELS            "regression;claslib"
)

# Step 2: Verify output is non-empty and positions match config
add_test(NAME claslib_ssr2osr_check
    COMMAND ${Python3_EXECUTABLE} -c "
import sys
lines=[l for l in open(sys.argv[1]) if l.startswith('$GPGGA') or l.startswith('$GNGGA')]
assert len(lines)>=100, f'too few GGA sentences: {len(lines)}'
for l in lines[:5]:
    f=l.split(',')
    lat=float(f[2][:2])+float(f[2][2:])/60.0
    lon=float(f[4][:3])+float(f[4][3:])/60.0
    assert abs(lat-36.1036)<0.01, f'lat {lat}'
    assert abs(lon-140.0863)<0.01, f'lon {lon}'
print(f'OK: {len(lines)} GGA sentences, position verified')
"
        ${CMAKE_BINARY_DIR}/out_ssr2osr.nmea
)
set_tests_properties(claslib_ssr2osr_check PROPERTIES
    DEPENDS           claslib_ssr2osr
    FIXTURES_REQUIRED claslib_data
    LABELS            "regression;claslib"
)

# ── rtkrcv RT CLAS PPP-RTK regression ─────────────────────────────────────────
# Replays BINEX obs + CLAS L6 via rtkrcv at x10 speed.
# Uses line-count threshold (≥90% of reference) for pass/fail.
add_test(NAME rtkrcv_rt_clas
    COMMAND bash ${_TESTCMAKE}/run_rtkrcv_test.sh
        $<TARGET_FILE:rtkrcv>
        ${CMAKE_SOURCE_DIR}
        ${_CLSDATA}/rtkrcv_clas_ref.nmea
        10
        conf/claslib/rtkrcv.conf
        52005
        500
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(rtkrcv_rt_clas PROPERTIES
    FIXTURES_REQUIRED claslib_data
    TIMEOUT           600
    LABELS            "regression;claslib;realtime"
    RESOURCE_LOCK     rtkrcv_port
)

# ═══════════════════════════════════════════════════════════════════════════════
# Tier 3 tests — position scatter (precision)
# ═══════════════════════════════════════════════════════════════════════════════
# No external reference is required.  The session centroid is computed from
# the solutions themselves, and 1σ (68th pctile) and 95% horizontal scatter
# must stay within the tolerance.  These catch gross regressions that leave
# the engine producing scattered or single-point solutions.
#
# Scatter checks are applied to the outputs already produced by Tier-1 runs:
#
#   madocalib_ppp_scatter
#       MIZU RINEX obs + MADOCA L6E (DOY 091 / 2025-04-01)  → MADOCA-PPP
#       Actual:  1σ=7.4 cm  95%=11.2 cm  (skip-epochs=30, fix-only)
#       Tolerance: 0.150 m  (≈2× actual)
#
#   claslib_ppp_rtk_2ch_scatter
#       0627 BINEX obs + dual-channel CLAS L6D (DOY 157 / 2025-06-06) → CLAS PPP-RTK
#       Actual:  1σ=5.8 cm  95%=25.2 cm  (skip-epochs=20, fix-only)
#       Tolerance: 0.400 m  (≈2× actual 95th; 2CH has ~88% fix rate)
#
# NOTE: Original cross-engine design (0627 obs → MADOCA-PPP; MIZU obs → CLAS
# PPP-RTK) required correction data from the same date as the observations.
# The prepared L6 files (2025157U.l6, 2025091A.{204,206}.l6) use a newer
# QZSS L6 encoding (MADOCA-PPPv2 / port-indexed format) that MRTKLIB does not
# yet decode.  Providing L6 data in the same format as 2025157U_ch{1,2}.l6
# (satellite-PRN-indexed, type-byte 0xA9/0xA3) would unblock those tests.

# ── madocalib_ppp_scatter ─────────────────────────────────────────────────────
add_test(NAME madocalib_ppp_scatter
    COMMAND ${Python3_EXECUTABLE} ${_CHECK_SCATTER}
        --tolerance 0.150
        --skip-epochs 30
        --fix-only
        ${CMAKE_BINARY_DIR}/out_madocalib_ppp.pos
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(madocalib_ppp_scatter PROPERTIES
    DEPENDS           madocalib_ppp
    FIXTURES_REQUIRED madocalib_data
    LABELS            "tier3;scatter;madocalib"
)

# ── claslib_ppp_rtk_2ch_scatter ───────────────────────────────────────────────
add_test(NAME claslib_ppp_rtk_2ch_scatter
    COMMAND ${Python3_EXECUTABLE} ${_CHECK_SCATTER}
        --tolerance 0.400
        --skip-epochs 20
        --fix-only
        ${CMAKE_BINARY_DIR}/out_claslib_ppp_rtk_2ch.nmea
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(claslib_ppp_rtk_2ch_scatter PROPERTIES
    DEPENDS           claslib_ppp_rtk_2ch
    FIXTURES_REQUIRED claslib_data
    LABELS            "tier3;scatter;claslib"
)

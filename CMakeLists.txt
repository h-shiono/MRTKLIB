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

# ── Core static library ──────────────────────────────────────────────────────
# Contains the union of all src/ files used by rnx2rtkp and rtkrcv.
# The linker pulls only the object files referenced by each executable.
add_library(mrtklib STATIC
    src/biassnx.c       src/ephemeris.c     src/geoid.c         src/ionex.c
    src/lambda.c        src/lclcmn.c        src/mdccssr.c       src/options.c
    src/pntpos.c        src/ppp.c           src/ppp_ar.c
    src/preceph.c       src/rcvraw.c        src/readfcb.c       src/rinex.c
    src/rtcm.c          src/rtcm2.c         src/rtcm3.c         src/rtcm3lcl.c
    src/rtcm3e.c        src/rtkcmn.c        src/rtkpos.c        src/rtksvr.c
    src/sbas.c          src/solution.c      src/stream.c        src/tides.c
    src/rcv/binex.c     src/rcv/crescent.c  src/rcv/javad.c     src/rcv/novatel.c
    src/rcv/nvs.c       src/rcv/rt17.c      src/rcv/septentrio.c src/rcv/skytraq.c
    src/rcv/ss2.c       src/rcv/ublox.c
)
target_include_directories(mrtklib PUBLIC src)
target_compile_definitions(mrtklib PUBLIC
    TRACE ENAGLO ENAGAL ENAQZS ENACMP ENAIRN NFREQ=5 NEXOBS=5
)
target_link_libraries(mrtklib PUBLIC m)
if(UNIX AND NOT APPLE)
    target_link_libraries(mrtklib PUBLIC rt)
endif()

# ── rnx2rtkp ─────────────────────────────────────────────────────────────────
# postpos.c uses PROGNAME so it must be compiled as part of this target
add_executable(rnx2rtkp
    app/consapp/rnx2rtkp/rnx2rtkp.c
    src/postpos.c
)
target_compile_definitions(rnx2rtkp PRIVATE PROGNAME=\"rnx2rtkp\")
target_link_libraries(rnx2rtkp PRIVATE mrtklib)

# ── rtkrcv ───────────────────────────────────────────────────────────────────
add_executable(rtkrcv
    app/consapp/rtkrcv/rtkrcv.c
    app/consapp/rtkrcv/vt.c
)
target_include_directories(rtkrcv PRIVATE app/consapp/rtkrcv)
target_compile_definitions(rtkrcv PRIVATE PROGNAME=\"rtkrcv\" SVR_REUSEADDR)
target_link_libraries(rtkrcv PRIVATE mrtklib pthread)

# ── Install ───────────────────────────────────────────────────────────────────
# cmake --install build  →  copies binaries to bin/ (matches make install)
install(TARGETS rnx2rtkp rtkrcv RUNTIME DESTINATION bin)

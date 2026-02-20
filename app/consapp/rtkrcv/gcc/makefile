# makefile for rtkrcv

PROGNAME=rtkrcv
BINDIR = ../../../../bin
SRC    = ../../../../src
LIBDIR  = ../../../../lib

OPTS  = -DTRACE -DENAGLO -DENAGAL -DENAQZS -DENACMP -DENAIRN -DNFREQ=5 -DNEXOBS=5 -DPROGNAME=\"$(PROGNAME)\" -DSVR_REUSEADDR
LIBS  =

# with Intel MKL
#MKLDIR = $(LIBDIR)/mkl/intel64
#OPTS  += -DMKL
#LIBS  += -L$(MKLDIR) -lmkl_intel_lp64 -lmkl_core -lmkl_gnu_thread -lgomp

CFLAGS = -Wall -O3 -ansi -pedantic -Wno-unused-but-set-variable -I$(SRC) $(OPTS) -g -I..
LDLIBS  = $(LIBS) -lm -lpthread

# Platform-specific settings
# -ansi defines __STRICT_ANSI__ which hides POSIX/BSD APIs in system headers.
# Use _DARWIN_C_SOURCE (macOS) or _GNU_SOURCE (Linux) to expose all APIs.
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -D_DARWIN_C_SOURCE
else ifeq ($(UNAME_S),Linux)
    CFLAGS += -D_GNU_SOURCE
    LDLIBS += -lrt
endif

all        : $(PROGNAME)
$(PROGNAME): $(PROGNAME).o vt.o rtksvr.o
$(PROGNAME): biassnx.o ephemeris.o geoid.o ionex.o lambda.o lclcmn.o mdccssr.o
$(PROGNAME): options.o pntpos.o ppp.o ppp_ar.o preceph.o rcvraw.o readfcb.o
$(PROGNAME): rinex.o rtcm.o rtcm2.o rtcm3.o rtcm3lcl.o rtcm3e.o rtkcmn.o
$(PROGNAME): rtkpos.o sbas.o solution.o stream.o tides.o binex.o
$(PROGNAME): crescent.o javad.o novatel.o nvs.o rt17.o septentrio.o skytraq.o
$(PROGNAME): ss2.o ublox.o

$(PROGNAME).o : ../$(PROGNAME).c
	$(CC) -c $(CFLAGS) ../$(PROGNAME).c
vt.o          : ../vt.c
	$(CC) -c $(CFLAGS) ../vt.c
rtksvr.o      : $(SRC)/rtksvr.c
	$(CC) -c $(CFLAGS) $(SRC)/rtksvr.c
biassnx.o     : $(SRC)/biassnx.c
	$(CC) -c $(CFLAGS) $(SRC)/biassnx.c
ephemeris.o   : $(SRC)/ephemeris.c
	$(CC) -c $(CFLAGS) $(SRC)/ephemeris.c
geoid.o       : $(SRC)/geoid.c
	$(CC) -c $(CFLAGS) $(SRC)/geoid.c
ionex.o       : $(SRC)/ionex.c
	$(CC) -c $(CFLAGS) $(SRC)/ionex.c
lambda.o      : $(SRC)/lambda.c
	$(CC) -c $(CFLAGS) $(SRC)/lambda.c
lclcmn.o      : $(SRC)/lclcmn.c
	$(CC) -c $(CFLAGS) $(SRC)/lclcmn.c
mdccssr.o     : $(SRC)/mdccssr.c
	$(CC) -c $(CFLAGS) $(SRC)/mdccssr.c
options.o     : $(SRC)/options.c
	$(CC) -c $(CFLAGS) $(SRC)/options.c
pntpos.o      : $(SRC)/pntpos.c
	$(CC) -c $(CFLAGS) $(SRC)/pntpos.c
ppp.o         : $(SRC)/ppp.c
	$(CC) -c $(CFLAGS) $(SRC)/ppp.c
ppp_ar.o      : $(SRC)/ppp_ar.c
	$(CC) -c $(CFLAGS) $(SRC)/ppp_ar.c
preceph.o     : $(SRC)/preceph.c
	$(CC) -c $(CFLAGS) $(SRC)/preceph.c
rcvraw.o      : $(SRC)/rcvraw.c
	$(CC) -c $(CFLAGS) $(SRC)/rcvraw.c
readfcb.o     : $(SRC)/readfcb.c
	$(CC) -c $(CFLAGS) $(SRC)/readfcb.c
rinex.o       : $(SRC)/rinex.c
	$(CC) -c $(CFLAGS) $(SRC)/rinex.c
rtcm.o        : $(SRC)/rtcm.c
	$(CC) -c $(CFLAGS) $(SRC)/rtcm.c
rtcm2.o       : $(SRC)/rtcm2.c
	$(CC) -c $(CFLAGS) $(SRC)/rtcm2.c
rtcm3.o       : $(SRC)/rtcm3.c
	$(CC) -c $(CFLAGS) $(SRC)/rtcm3.c
rtcm3e.o      : $(SRC)/rtcm3e.c
	$(CC) -c $(CFLAGS) $(SRC)/rtcm3e.c
rtcm3lcl.o   : $(SRC)/rtcm3lcl.c
	$(CC) -c $(CFLAGS) $(SRC)/rtcm3lcl.c
rtkcmn.o      : $(SRC)/rtkcmn.c
	$(CC) -c $(CFLAGS) $(SRC)/rtkcmn.c
rtkpos.o      : $(SRC)/rtkpos.c
	$(CC) -c $(CFLAGS) $(SRC)/rtkpos.c
sbas.o        : $(SRC)/sbas.c
	$(CC) -c $(CFLAGS) $(SRC)/sbas.c
solution.o    : $(SRC)/solution.c
	$(CC) -c $(CFLAGS) $(SRC)/solution.c
stream.o      : $(SRC)/stream.c
	$(CC) -c $(CFLAGS) $(SRC)/stream.c
tides.o       : $(SRC)/tides.c
	$(CC) -c $(CFLAGS) $(SRC)/tides.c
binex.o       : $(SRC)/rcv/binex.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/binex.c
crescent.o    : $(SRC)/rcv/crescent.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/crescent.c
javad.o       : $(SRC)/rcv/javad.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/javad.c
novatel.o     : $(SRC)/rcv/novatel.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/novatel.c
nvs.o         : $(SRC)/rcv/nvs.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/nvs.c
rt17.o        : $(SRC)/rcv/rt17.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/rt17.c
septentrio.o  : $(SRC)/rcv/septentrio.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/septentrio.c
skytraq.o     : $(SRC)/rcv/skytraq.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/skytraq.c
ss2.o         : $(SRC)/rcv/ss2.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/ss2.c
ublox.o       : $(SRC)/rcv/ublox.c
	$(CC) -c $(CFLAGS) $(SRC)/rcv/ublox.c

$(PROGNAME).o : $(SRC)/rtklib.h ../vt.h
vt.o          : ../vt.h
rtksvr.o      : $(SRC)/rtklib.h
biassnx.o     : $(SRC)/rtklib.h
ephemeris.o   : $(SRC)/rtklib.h
geoid.o       : $(SRC)/rtklib.h
ionex.o       : $(SRC)/rtklib.h
lambda.o      : $(SRC)/rtklib.h
lclcmn.o      : $(SRC)/rtklib.h
mdccssr.o     : $(SRC)/rtklib.h
options.o     : $(SRC)/rtklib.h
pntpos.o      : $(SRC)/rtklib.h
ppp.o         : $(SRC)/rtklib.h
ppp_ar.o      : $(SRC)/rtklib.h
preceph.o     : $(SRC)/rtklib.h
rcvraw.o      : $(SRC)/rtklib.h
readfcb.o     : $(SRC)/rtklib.h
rinex.o       : $(SRC)/rtklib.h
rtcm.o        : $(SRC)/rtklib.h
rtcm2.o       : $(SRC)/rtklib.h
rtcm3.o       : $(SRC)/rtklib.h
rtcm3lcl.o    : $(SRC)/rtklib.h
rtcm3e.o      : $(SRC)/rtklib.h
rtkcmn.o      : $(SRC)/rtklib.h
rtkpos.o      : $(SRC)/rtklib.h
sbas.o        : $(SRC)/rtklib.h
solution.o    : $(SRC)/rtklib.h
stream.o      : $(SRC)/rtklib.h
tides.o       : $(SRC)/rtklib.h
binex.o       : $(SRC)/rtklib.h
crescent.o    : $(SRC)/rtklib.h
javad.o       : $(SRC)/rtklib.h
novatel.o     : $(SRC)/rtklib.h
nvs.o         : $(SRC)/rtklib.h
rt17.o        : $(SRC)/rtklib.h
septentrio.o  : $(SRC)/rtklib.h
skytraq.o     : $(SRC)/rtklib.h
ss2.o         : $(SRC)/rtklib.h
ublox.o       : $(SRC)/rtklib.h

clean:
	rm -f $(PROGNAME) $(PROGNAME).exe $(PROGNAME).nav *.o *.out *.trace

install :
	cp $(PROGNAME) $(BINDIR)

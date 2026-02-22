# makefile for recvbias

PROGNAME=recvbias
BINDIR  = ../../../../bin
SRC     = ../../../../src
LIBDIR  = ../../../../lib

OPTS  = -DTRACE -DENAGLO -DENAQZS -DENAGAL -DENACMP -DENAIRN -DNFREQ=5 -DNEXOBS=5 -DPROGNAME=\"$(PROGNAME)\"
LIBS  =

CFLAGS = -Wall -O3 -ansi -pedantic -Wno-unused-but-set-variable -I$(SRC) $(OPTS) -g -O -std=c99
LDLIBS = $(LIBS) -lm

# Linux requires -lrt for clock_gettime; macOS does not
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDLIBS += -lrt
endif

all        : $(PROGNAME)
$(PROGNAME): $(PROGNAME).o biassnx.o ephemeris.o ionex.o lclcmn.o mdccssr.o
$(PROGNAME): preceph.o rinex.o rtcm.o rtcm2.o rtcm3.o rtcm3lcl.o rtcm3e.o
$(PROGNAME): rtkcmn.o sbas.o

$(PROGNAME).o : ../$(PROGNAME).c
	$(CC) -c $(CFLAGS) ../$(PROGNAME).c
biassnx.o     : $(SRC)/biassnx.c
	$(CC) -c $(CFLAGS) $(SRC)/biassnx.c
ephemeris.o   : $(SRC)/ephemeris.c
	$(CC) -c $(CFLAGS) $(SRC)/ephemeris.c
ionex.o       : $(SRC)/ionex.c
	$(CC) -c $(CFLAGS) $(SRC)/ionex.c
lclcmn.o      : $(SRC)/lclcmn.c
	$(CC) -c $(CFLAGS) $(SRC)/lclcmn.c
mdccssr.o     : $(SRC)/mdccssr.c
	$(CC) -c $(CFLAGS) $(SRC)/mdccssr.c
preceph.o     : $(SRC)/preceph.c
	$(CC) -c $(CFLAGS) $(SRC)/preceph.c
rinex.o       : $(SRC)/rinex.c
	$(CC) -c $(CFLAGS) $(SRC)/rinex.c
rtcm.o        : $(SRC)/rtcm.c
	$(CC) -c $(CFLAGS) $(SRC)/rtcm.c
rtcm2.o       : $(SRC)/rtcm2.c
	$(CC) -c $(CFLAGS) $(SRC)/rtcm2.c
rtcm3.o       : $(SRC)/rtcm3.c
	$(CC) -c $(CFLAGS) $(SRC)/rtcm3.c
rtcm3lcl.o    : $(SRC)/rtcm3lcl.c
	$(CC) -c $(CFLAGS) $(SRC)/rtcm3lcl.c
rtcm3e.o      : $(SRC)/rtcm3e.c
	$(CC) -c $(CFLAGS) $(SRC)/rtcm3e.c
rtkcmn.o      : $(SRC)/rtkcmn.c
	$(CC) -c $(CFLAGS) $(SRC)/rtkcmn.c
sbas.o        : $(SRC)/sbas.c
	$(CC) -c $(CFLAGS) $(SRC)/sbas.c

$(PROGNAME).o : $(SRC)/rtklib.h
biassnx.o     : $(SRC)/rtklib.h
ephemeris.o   : $(SRC)/rtklib.h
ionex.o       : $(SRC)/rtklib.h
lclcmn.o      : $(SRC)/rtklib.h
mdccssr.o     : $(SRC)/rtklib.h
preceph.o     : $(SRC)/rtklib.h
rinex.o       : $(SRC)/rtklib.h
rtcm.o        : $(SRC)/rtklib.h
rtcm2.o       : $(SRC)/rtklib.h
rtcm3.o       : $(SRC)/rtklib.h
rtcm3lcl.o    : $(SRC)/rtklib.h
rtcm3e.o      : $(SRC)/rtklib.h
rtkcmn.o      : $(SRC)/rtklib.h
sbas.o        : $(SRC)/rtklib.h

clean :
	rm -f $(PROGNAME) $(PROGNAME).exe *.o

install :
	cp -p $(PROGNAME) $(BINDIR)

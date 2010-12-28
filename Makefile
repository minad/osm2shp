CPP=g++
CC=gcc
CFLAGS=-O2 -Wall

all: osmfilter osmchange osm2shp

osm2shp: osm2shp.o
	$(CPP) $(CFLAGS) -lexpat -lsqlite3 -lshp -lboost_iostreams -o $@ $+

osmfilter: osmfilter.o
	$(CC) $(CFLAGS) -o $@ $+

osmchange: osmchange.o
	$(CC) $(CFLAGS) -o $@ $+

%.o: %.cc
	$(CPP) $(CFLAGS) -c $<

clean:
	rm -f *.o

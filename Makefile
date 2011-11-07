CPP=g++
CC=gcc
CFLAGS=-O2 -Wall

all: osm2shp

osm2shp: osm2shp.o
	$(CPP) $(CFLAGS) $+ -lexpat -lsqlite3 -lshp -lboost_iostreams -o $@

%.o: %.cc
	$(CPP) $(CFLAGS) -c $<

clean:
	rm -f *.o

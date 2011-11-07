CPP=g++
CC=gcc
CFLAGS=-O2 -Wall

all: osm2shp

FILES = \
  osm2shp.o \
  osm/shapefile.o \
  osm/point_database.o

osm2shp: $(FILES)
	$(CPP) $(CFLAGS) $+ -lexpat -lsqlite3 -lshp -lboost_iostreams -o $@

%.o: %.cc
	$(CPP) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(FILES) osm2shp

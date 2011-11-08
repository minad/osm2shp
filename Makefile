CPP=g++
CC=gcc
CFLAGS=-O2 -Wall -Wredundant-decls 

all: osm2shp

LIB_GEOS     = $(shell geos-config --libs)
LIB_SHAPE    = -lshp $(LIB_GEOS)
LIB_PROTOBUF = -lz -lprotobuf-lite -losmpbf

FILES = \
  osm2shp.o \
  osm/shapefile.o \
  osm/handler.o \
  osm/point_database.o

osm2shp: $(FILES)
	$(CPP) $(CFLAGS) $+ -lexpat -lsqlite3 -lshp -lboost_iostreams $(LIB_PROTOBUF) $(LIB_SHAPE) -o $@

%.o: %.cc
	$(CPP) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(FILES) osm2shp

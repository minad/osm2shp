CPP = g++

CXXFLAGS = -ggdb -Wall -Wredundant-decls
CXXFLAGS += -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

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
	$(CPP) $(CXXFLAGS) $+ -lexpat -lsqlite3 -lshp $(LIB_PROTOBUF) $(LIB_SHAPE) -o $@

%.o: %.cc
	$(CPP) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(FILES) osm2shp

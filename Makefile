CPP=g++
CFLAGS=-O2 -Wall
LDFLAGS=-lexpat -lsqlite3 -lshp -lboost_iostreams

osm2shp: osm2shp.o
	$(CPP) $(CFLAG) $(LDFLAGS) -o $@ $+

%.o: %.cc
	$(CPP) $(CFLAGS) $(LDFLAGS) -c $<

clean:
	rm -f *.o

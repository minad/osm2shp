#! /bin/sh

OSM_PATH=germany.osm.gz
CONVERTED_PATH=germany
DEST_PATH=shp

rm -f $DEST_PATH/*
mkdir -p $DEST_PATH

if [ ! -e $CONVERTED_PATH ]; then
    ./osm2shp $OSM_PATH $CONVERTED_PATH
fi

cp $CONVERTED_PATH/city* $DEST_PATH

g.remove vect=roadbig_line,roadbig_line1,roadbig_line2,roadbig_line3
v.in.ogr -t dsn=$CONVERTED_PATH layer=roadbig_line output=roadbig_line1
v.build.polylines input=roadbig_line1 output=roadbig_line2
v.generalize input=roadbig_line2 output=roadbig_line3 method=douglas threshold=0.002
v.clean input=roadbig_line3 output=roadbig_line tool=snap,break,rmdupl thres=0.002
g.remove vect=roadbig_line1,roadbig_line2,roadbig_line3
v.out.ogr input=roadbig_line type=line dsn=$DEST_PATH/roadbig_line.shp

g.remove vect=roadmedium_line,roadmedium_line1,roadmedium_line2,roadmedium_line3
v.in.ogr -t dsn=$CONVERTED_PATH layer=roadmedium_line output=roadmedium_line1
v.build.polylines input=roadmedium_line1 output=roadmedium_line2
v.generalize input=roadmedium_line2 output=roadmedium_line3 method=douglas threshold=0.002
v.clean input=roadmedium_line3 output=roadmedium_line tool=snap,break,rmdupl thres=0.002
g.remove vect=roadmedium_line1,roadmedium_line2,roadmedium_line3
v.out.ogr input=roadmedium_line type=line dsn=$DEST_PATH/roadmedium_line.shp

g.remove vect=roadsmall_line,roadsmall_line1,roadsmall_line2,roadsmall_line3
v.in.ogr -t dsn=$CONVERTED_PATH layer=roadsmall_line output=roadsmall_line1
v.build.polylines input=roadsmall_line1 output=roadsmall_line2
v.generalize input=roadsmall_line2 output=roadsmall_line3 method=douglas threshold=0.005
v.clean input=roadsmall_line3 output=roadsmall_line tool=snap,break,rmdupl thres=0.005
g.remove vect=roadsmall_line1,roadsmall_line2,roadsmall_line3
v.out.ogr input=roadsmall_line type=line dsn=$DEST_PATH/roadsmall_line.shp

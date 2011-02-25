#! /bin/sh

if [ $# -ne 1 ]; then
    echo "usage: $0 base-name"
    echo ""
    echo "Generate shapefile directory 'planet' from 'planet.osm*' with '$0 planet'."
    exit 1
fi

NAME=$1

CONVERTED_PATH=$NAME.converted
DEST_PATH=$NAME

remove_converted() {
    rm -rf $CONVERTED_PATH
    exit 1
}

if [ -e $CONVERTED_PATH ]; then
    echo "================================================================================"
    echo "Using already converted osm shapefiles from directory $CONVERTED_PATH"
else
    if [ -e "$NAME.osm" ]; then
	OSM_PATH=$NAME.osm
    elif [ -e "$NAME.osm.gz" ]; then
	OSM_PATH=$NAME.osm.gz
    elif [ -e "$NAME.osm.bz2" ]; then
	OSM_PATH=$NAME.osm.bz2
    else
	echo "$NAME.osm(.gz|.bz2) not found"
	exit 1
    fi

    echo "================================================================================"
    echo "Converting $OSM_PATH to shapefiles in directory $CONVERTED_PATH"
    echo "================================================================================"
    trap remove_converted INT
    ./osm2shp $OSM_PATH $CONVERTED_PATH
fi

echo "================================================================================"
echo "Postprocess shapefiles with GRASS GIS - Destination directory $DEST_PATH"
echo "================================================================================"

if [ "$GISBASE" = "" ]; then
    echo "You must be in GRASS GIS to continue with this step."
    echo "Restart this program in GRASS GIS to continue."
    exit 1
fi

rm -f $DEST_PATH/*
mkdir -p $DEST_PATH
cp $CONVERTED_PATH/*_point* $DEST_PATH

g.remove vect=roadbig_line,roadbig_line1,roadbig_line2,roadbig_line3,roadbig_line4
v.in.ogr -t dsn=$CONVERTED_PATH layer=roadbig_line output=roadbig_line1
v.build.polylines input=roadbig_line1 output=roadbig_line2
v.generalize input=roadbig_line2 output=roadbig_line3 method=douglas threshold=0.002
v.clean input=roadbig_line3 output=roadbig_line4 tool=snap,break,rmdupl thres=0.002
v.clean input=roadbig_line4 output=roadbig_line tool=rmline type=line
g.remove vect=roadbig_line1,roadbig_line2,roadbig_line3,roadbig_line4
v.out.ogr input=roadbig_line type=line dsn=$DEST_PATH/roadbig_line.shp

g.remove vect=roadmedium_line,roadmedium_line1,roadmedium_line2,roadmedium_line3,roadmedium_line4
v.in.ogr -t dsn=$CONVERTED_PATH layer=roadmedium_line output=roadmedium_line1
v.build.polylines input=roadmedium_line1 output=roadmedium_line2
v.generalize input=roadmedium_line2 output=roadmedium_line3 method=douglas threshold=0.002
v.clean input=roadmedium_line3 output=roadmedium_line4 tool=snap,break,rmdupl thres=0.002
v.clean input=roadmedium_line4 output=roadmedium_line tool=rmline type=line
g.remove vect=roadmedium_line1,roadmedium_line2,roadmedium_line3,roadmedium_line4
v.out.ogr input=roadmedium_line type=line dsn=$DEST_PATH/roadmedium_line.shp

g.remove vect=roadsmall_line,roadsmall_line1,roadsmall_line2,roadsmall_line3,roadsmall_line4
v.in.ogr -t dsn=$CONVERTED_PATH layer=roadsmall_line output=roadsmall_line1
v.build.polylines input=roadsmall_line1 output=roadsmall_line2
v.generalize input=roadsmall_line2 output=roadsmall_line3 method=douglas threshold=0.005
v.clean input=roadsmall_line3 output=roadsmall_line4 tool=snap,break,rmdupl thres=0.005
v.clean input=roadsmall_line4 output=roadsmall_line tool=rmline type=line
g.remove vect=roadsmall_line1,roadsmall_line2,roadsmall_line3,roadsmall_line4
v.out.ogr input=roadsmall_line type=line dsn=$DEST_PATH/roadsmall_line.shp

g.remove vect=railway_line,railway_line1,railway_line2,railway_line3,railway_line4
v.in.ogr -t dsn=$CONVERTED_PATH layer=railway_line output=railway_line1
v.build.polylines input=railway_line1 output=railway_line2
v.generalize input=railway_line2 output=railway_line3 method=douglas threshold=0.002
v.clean input=railway_line3 output=railway_line4 tool=snap,break,rmdupl thres=0.002
v.clean input=railway_line4 output=railway_line tool=rmline type=line
g.remove vect=railway_line1,railway_line2,railway_line3,railway_line4
v.out.ogr input=railway_line type=line dsn=$DEST_PATH/railway_line.shp

echo "================================================================================"
echo "Finished postprocessing. Processed shapefiles are in directory $DEST_PATH"
echo "================================================================================"

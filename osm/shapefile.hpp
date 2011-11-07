#ifndef OSM2SHP_OSM_SHAPEFILE_HPP
#define OSM2SHP_OSM_SHAPEFILE_HPP

#include <shapefil.h>
#include <boost/utility.hpp>

namespace osm {

class shape_file : boost::noncopyable {
public:
        explicit shape_file(const std::string& name, int type);
        ~shape_file();

        int type() const {
                return type_;
        }

        void add_field(const char* name);
        void add_attribute(int field, const std::string& value);
        void point(double x, double y);
        void multipoint(int type, size_t size, const double* x, const double* y);

private:
        void open_shp();
        void open_dbf();
        void create_prj();

        std::string name_;
        int         type_;
        SHPHandle   shp_;
        DBFHandle   dbf_;
        int         current_id_;
};

}

#endif

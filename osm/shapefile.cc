#include "shapefile.hpp"

#include <fstream>
#include <assert.h>
#include <stdexcept>

namespace osm {

shape_file::shape_file(const std::string& name, int type)
        : name_(name), type_(type), shp_(0), dbf_(0), current_id_(-1) {
        open_shp();
        create_prj();
}

shape_file::~shape_file() {
        SHPClose(shp_);
        if (dbf_)
                DBFClose(dbf_);
}

void shape_file::add_field(const char* name) {
        open_dbf();
        DBFAddField(dbf_, name, FTString, 64, 0);
}

void shape_file::add_attribute(int field, const std::string& value) {
        assert(current_id_ >= 0);
        assert(dbf_);
        DBFWriteStringAttribute(dbf_, current_id_, field, value.c_str());
}

void shape_file::point(double x, double y) {
        assert(type_ == SHPT_POINT);
        SHPObject* obj = SHPCreateSimpleObject(SHPT_POINT, 1, &x, &y, 0);
        current_id_ = SHPWriteObject(shp_, -1, obj);
        SHPDestroyObject(obj);
        if (current_id_ < 0)
                throw std::runtime_error("failed to write point object");
}

void shape_file::multipoint(int type, size_t size, const double* x, const double* y) {
        assert(type_ == type);
        SHPObject* obj = SHPCreateSimpleObject(type, size,
                                               const_cast<double*>(x), const_cast<double*>(y), 0);
        current_id_ = SHPWriteObject(shp_, -1, obj);
        SHPDestroyObject(obj);
        if (current_id_ < 0)
                throw std::runtime_error("failed to write multipoint object");
}

void shape_file::open_shp() {
        shp_ = SHPCreate(name_.c_str(), type_);
        if (!shp_)
                throw std::runtime_error("Could not open shapefile " + name_);
}

void shape_file::open_dbf() {
        if (!dbf_) {
                dbf_ = DBFCreate(name_.c_str());
                if (!dbf_)
                        throw std::runtime_error("Could not open dbf file " + name_);
        }
}

void shape_file::create_prj() {
        std::ofstream out((name_ + ".prj").c_str());
        out << "GEOGCS[\"WGS 84\",\n"
               "       DATUM[\"WGS_1984\",\n"
               "            SPHEROID[\"WGS 84\",6378137,298.257223563,\n"
               "                      AUTHORITY[\"EPSG\",\"7030\"]],\n"
               "            TOWGS84[0,0,0,0,0,0,0],\n"
               "             AUTHORITY[\"EPSG\",\"6326\"]],\n"
               "       PRIMEM[\"Greenwich\",0,\n"
               "              AUTHORITY[\"EPSG\",\"8901\"]],\n"
               "       UNIT[\"degree\",0.0174532925199433,\n"
               "            AUTHORITY[\"EPSG\",\"9108\"]],\n"
               "       AUTHORITY[\"EPSG\",\"4326\"]]";
        out.close();
}

}

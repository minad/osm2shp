#include "handler.hpp"
#include "shapefile.hpp"

#include <shapefil.h>
#include <sys/stat.h>
#include <boost/foreach.hpp>
#include <boost/format.hpp>

#define foreach BOOST_FOREACH

namespace osm {

template<typename T, class K>
inline bool has_key(const T& map, const K& key) {
		const char *v = map.get_value_by_key(key);
		return v;
}

template<typename T, class K, class V>
inline bool has_key_value(const T& map, const K& key, const V& value) {
		const char *v = map.get_value_by_key(key);
		return v && !strcmp(v, value);
}

handler::handler(const std::string& base)
        : tmp_nodes_(boost::str(boost::format("tmpnodes-%1%.sqlite") % getpid())),
          processed_nodes_(0), processed_ways_(0),
          exported_nodes_(0), exported_ways_(0),
          base_path_(base) {

        mkdir(base.c_str(), 0755);

        add_shape("roadbig_line",     SHPT_ARC);
        add_shape("roadmedium_line",  SHPT_ARC);
        add_shape("roadsmall_line",   SHPT_ARC);
        add_shape("railway_line",     SHPT_ARC);
        add_shape("city_point",       SHPT_POINT);
        add_shape("town_point",       SHPT_POINT);
        add_shape("suburb_point",     SHPT_POINT);
        add_shape("village_point",    SHPT_POINT);
        add_shape("water_line",       SHPT_ARC);
        add_shape("water_area",       SHPT_POLYGON);

        add_layer("roadbig_line",     "highway",  "motorway");
        add_layer("roadbig_line",     "highway",  "trunk");
        add_layer("roadmedium_line",  "highway",  "primary");
        add_layer("roadsmall_line" ,  "highway",  "secondary");
        add_layer("railway_line",     "railway",  "rail");
        add_layer("city_point",       "place",    "city");
        add_layer("town_point",       "place",    "town");
        add_layer("suburb_point",     "place",    "suburb");
        add_layer("village_point",    "place",    "village");
        add_layer("water_line",       "waterway", "river");
        add_layer("water_line",       "waterway", "canal");
        add_layer("water_area",       "natural",  "water");
}

handler::~handler() {
        std::cout << "Total exported nodes: "   << exported_nodes_
                  << "\nTotal exported ways:  " << exported_ways_ << std::endl;

        foreach (shape_map::value_type& value, shapes_)
                delete value.second;
}

void handler::add_shape(const std::string& name, int type) {
        shapes_[name] = new shape_file(base_path_ + "/" + name, type);
        if (type == SHPT_POINT)
                shapes_[name]->add_field("name");
}

void handler::add_layer(const std::string& name, const std::string& type, const std::string& subtype) {
        shape_file* shape = shapes_[name];
        assert(shape);
        layers_.push_back(layer(shape, type, subtype));
}

void handler::node(const shared_ptr<Osmium::OSM::Node const>& node) {
		int64_t id_ = node->id();
		double x_  = node->position().lon();
		double y_  = node->position().lat();

        if (++processed_nodes_ % 100000 == 0)
                std::cout << processed_nodes_ << " nodes processed, " << exported_nodes_ << " nodes exported" << std::endl;

        if (id_ <= 0)
                return;

        tmp_nodes_.set(id_, x_, y_);

        const char* name = node->tags().get_value_by_key("name");
        if (!name)
                return;

        foreach (const layer& lay, layers_) {
                if (lay.shape()->type() == SHPT_POINT &&
                    has_key_value(node->tags(), lay.type().c_str(), lay.subtype().c_str())) {
                        lay.shape()->point(x_, y_);
                        lay.shape()->add_attribute(0, name);
                        ++exported_nodes_;
                        break;
                }
        }
}

void handler::way(const shared_ptr<Osmium::OSM::Way>& way) {
        if (++processed_ways_ % 10000 == 0)
                std::cout << processed_ways_ << " ways processed, " << exported_ways_ << " ways exported" << std::endl;

        int type = is_area(way) ? SHPT_POLYGON : SHPT_ARC;
        if ((type == SHPT_POLYGON && way->nodes().size() < 3) || way->nodes().size() < 2)
                return;

        foreach (const layer& lay, layers_) {
                if (lay.shape()->type() == type && has_key_value(way->tags(), lay.type().c_str(), lay.subtype().c_str())) {
                        double x[way->nodes().size()], y[way->nodes().size()];
                        if (tmp_nodes_.get(way->nodes(), x, y)) {
                                lay.shape()->multipoint(type, way->nodes().size(), x, y);
                                ++exported_ways_;
                        }
                        break;
                }
        }
}

bool handler::is_area(const shared_ptr<Osmium::OSM::Way>& way) {
        return has_key_value(way->tags(), "area", "yes")      ||
               has_key(way->tags(), "landuse")                ||
               has_key_value(way->tags(), "natural", "land")  ||
               has_key_value(way->tags(), "natural", "water") ||
               has_key_value(way->tags(), "natural", "woord");
}

}

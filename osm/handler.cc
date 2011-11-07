#include "handler.hpp"
#include "shapefile.hpp"

#include <shapefil.h>
#include <sys/stat.h>
#include <boost/foreach.hpp>

#define foreach BOOST_FOREACH

namespace osm {

template<typename T>
inline bool has_key(const T& map, const typename T::key_type& key) {
        return map.find(key) != map.end();
}

template<typename T, class K, class V>
inline bool has_key_value(const T& map, const K& key, const V& value) {
        typename T::const_iterator i = map.find(key);
        return i != map.end() && i->second == value;
}

handler::handler(const std::string& base)
        : tmp_nodes_(boost::str(boost::format("tmpnodes-%1%.sqlite") % getpid())),
          processed_nodes_(0), processed_ways_(0),
          exported_nodes_(0), exported_ways_(0),
          base_path_(base), taggable_(false) {

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

void handler::start_element(const xml::string& name, const xml::attributes& attr) {
        if (name == "node")
                start_node(attr);
        else if (name == "way")
                start_way(attr);
        else if (name == "nd")
                start_nd(attr);
        else if (taggable_ && name == "tag")
                start_tag(attr);
}

void handler::end_element(const xml::string& name) {
        if (name == "node")
                end_node();
        else if (name == "way")
                end_way();
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

void handler::start_node(const xml::attributes& attr) {
        taggable_ = true;
        tags_.clear();
        id_ = attr.as_int64("id");
        x_  = attr.as_double("lon");
        y_  = attr.as_double("lat");
}

void handler::start_way(const xml::attributes& attr) {
        taggable_ = true;
        tags_.clear();
        nodes_.clear();
}

void handler::start_nd(const xml::attributes& attr) {
        nodes_.push_back(attr.as_int64("ref"));
}

void handler::start_tag(const xml::attributes& attr) {
        tags_[attr["k"]] = attr["v"];
}

void handler::end_node() {
        taggable_ = false;

        if (++processed_nodes_ % 100000 == 0)
                std::cout << processed_nodes_ << " nodes processed, " << exported_nodes_ << " nodes exported" << std::endl;

        if (id_ <= 0)
                return;

        tmp_nodes_.set(id_, x_, y_);

        tag_map::const_iterator i = tags_.find("name");
        if (i == tags_.end())
                return;

        foreach (const layer& lay, layers_) {
                if (lay.shape()->type() == SHPT_POINT &&
                    has_key_value(tags_, lay.type(), lay.subtype())) {
                        lay.shape()->point(x_, y_);
                        lay.shape()->add_attribute(0, i->second);
                        ++exported_nodes_;
                        break;
                }
        }
}

void handler::end_way() {
        taggable_ = false;

        if (++processed_ways_ % 10000 == 0)
                std::cout << processed_ways_ << " ways processed, " << exported_ways_ << " ways exported" << std::endl;

        int type = is_area() ? SHPT_POLYGON : SHPT_ARC;
        if ((type == SHPT_POLYGON && nodes_.size() < 3) || nodes_.size() < 2)
                return;

        foreach (const layer& lay, layers_) {
                if (lay.shape()->type() == type && has_key_value(tags_, lay.type(), lay.subtype())) {
                        double x[nodes_.size()], y[nodes_.size()];
                        if (tmp_nodes_.get(nodes_, x, y)) {
                                lay.shape()->multipoint(type, nodes_.size(), x, y);
                                ++exported_ways_;
                        }
                        break;
                }
        }
}

bool handler::is_area() {
        return has_key_value(tags_, "area", "yes")      ||
               has_key(tags_, "landuse")                ||
               has_key_value(tags_, "natural", "land")  ||
               has_key_value(tags_, "natural", "water") ||
               has_key_value(tags_, "natural", "woord");
}

}

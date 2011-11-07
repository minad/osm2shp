#ifndef OSM2SHP_OSM_HANDLER_HPP
#define OSM2SHP_OSM_HANDLER_HPP

#include "point_database.hpp"
#include "layer.hpp"
#include "../xml.hpp"

#include <map>
#include <vector>

namespace osm {

class shape_file;

class handler {
        typedef std::map<std::string, shape_file*> shape_map;
        typedef std::map<std::string, std::string> tag_map;

public:

        handler(const std::string& base);
        ~handler();

        void start_element(const xml::string& name, const xml::attributes& attr);
        void end_element(const xml::string& name);

private:

        void add_shape(const std::string& name, int type);
        void add_layer(const std::string& name, const std::string& type, const std::string& subtype);
        void start_node(const xml::attributes& attr);
        void start_way(const xml::attributes& attr);
        void start_nd(const xml::attributes& attr);
        void start_tag(const xml::attributes& attr);
        void end_node();
        void end_way();
        bool is_area();

        point_database       tmp_nodes_;
        int64_t              processed_nodes_;
        int64_t              processed_ways_;
        int64_t              exported_nodes_;
        int64_t              exported_ways_;
        std::vector<layer>   layers_;
        std::vector<int64_t> nodes_;
        std::string          base_path_;
        bool                 taggable_;
        tag_map              tags_;
        int64_t              id_;
        double               x_, y_;
        shape_map            shapes_;
};

}

#endif

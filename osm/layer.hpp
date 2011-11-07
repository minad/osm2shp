#ifndef OSM2SHP_OSM_LAYER_HPP
#define OSM2SHP_OSM_LAYER_HPP

#include <string>

namespace osm {

class shape_file;

class layer {
public:
        layer(shape_file* shape, const std::string& type, const std::string& subtype)
                : shape_(shape), type_(type), subtype_(subtype) {
        }

        const std::string& type() const {
                return type_;
        }

        const std::string& subtype() const {
                return subtype_;
        }

        shape_file* shape() const {
                return shape_;
        }

private:
        shape_file* shape_;
        std::string type_;
        std::string subtype_;
};

}

#endif

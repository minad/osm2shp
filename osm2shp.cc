#define OSMIUM_WITH_PBF_INPUT
#define OSMIUM_WITH_XML_INPUT


#include <osmium.hpp>
#include <osmium/input.hpp>

#include "osm/handler.hpp"

int main(int argc, char* argv[]) {
        try {
                if (argc != 3) {
                        std::cerr << "usage: " << argv[0] << " planet.osm(.gz|.bz2) base-path" << std::endl;
                        return 1;
                }
                Osmium::OSMFile  infile(argv[1]);
                osm::handler handler(argv[2]);
                Osmium::Input::read(infile, handler);
                return 0;
        } catch (const std::exception& ex) {
                std::cerr << ex.what() << std::endl;
                return 1;
        }
}

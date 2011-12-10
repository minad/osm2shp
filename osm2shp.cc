#include "osm/handler.hpp"

#include <osmium.hpp>

int main(int argc, char* argv[]) {
        try {
                if (argc != 3) {
                        std::cerr << "usage: " << argv[0] << " planet.osm(.gz|.bz2) base-path" << std::endl;
                        return 1;
                }

                Osmium::init(true);

                Osmium::OSMFile infile(argv[1]);
                osm::handler handler(argv[2]);
                infile.read(handler);
                return 0;
        } catch (const std::exception& ex) {
                std::cerr << ex.what() << std::endl;
                return 1;
        }
}

#include "osm/handler.hpp"
#include "xml.hpp"

int main(int argc, char* argv[]) {
        try {
                if (argc != 3) {
                        std::cerr << "usage: " << argv[0] << " planet.osm(.gz|.bz2) base-path" << std::endl;
                        return 1;
                }
                osm::handler handler(argv[2]);
                xml::parser<osm::handler> parser(handler);
                parser.parse(argv[1]);
                return 0;
        } catch (const std::exception& ex) {
                std::cerr << ex.what() << std::endl;
                return 1;
        }
}

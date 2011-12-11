#ifndef OSM2SHP_OSM_POINT_DATABASE_HPP
#define OSM2SHP_OSM_POINT_DATABASE_HPP

#include <sqlite3.h>
#include <vector>
#include <boost/utility.hpp>
#include <stdexcept>

#include <osmium/osm/way_node_list.hpp>

namespace osm {

struct point_database : boost::noncopyable {
        point_database(const std::string& name);
        ~point_database();

        const std::string& name() const {
                return name_;
        }

        void set(int64_t id, double x, double y);
        bool get(const Osmium::OSM::WayNodeList& way_node_list, double* x_result, double* y_result);

private:

        struct stmt_wrapper {
                sqlite3_stmt* stmt;

                stmt_wrapper()
                        : stmt(0) {
                }

                operator sqlite3_stmt*() {
                        return stmt;
                }

                ~stmt_wrapper() {
                        sqlite3_finalize(stmt);
                }
        };

        void close();
        sqlite3_stmt* build_fetch_stmt(int how_many);
        void exec(const char* sql);

        void db_error(const char* msg) {
                throw std::runtime_error(std::string(msg) + ": " + sqlite3_errmsg(db_));
        }

        std::string   name_;
        sqlite3*      db_;
        sqlite3_stmt* ins_stmt_;
};

}

#endif

#include <sqlite3.h>
#include <libshp/shapefil.h>
#include <stdint.h>
#include <expat.h>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <boost/foreach.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filtering_stream.hpp>

#define foreach BOOST_FOREACH

namespace xml {

class parse_error: public std::runtime_error {
public:
        parse_error(const std::string& what, int where)
                : std::runtime_error(what), where_(where) {
        }

        int where() const {
                return where_;
        }

private:
        int where_;
};

class string {
public:

        string(const char* str)
                : str_(str) {
        }

        bool operator==(const char* s) const {
                return !strcmp(str_, s);
        }

        const char* c_str() const {
                return str_;
        }

private:
        const char* str_;
};

class attributes {
public:

        explicit attributes(const char** attr)
                : attr_(attr) {
        }

        const char* operator[](const xml::string& key) const {
                return get(key);
        }

        const char* get(const xml::string& key) const {
                for (int i = 0; attr_[i]; i += 2) {
                        if (key == attr_[i])
                                return attr_[i + 1];
                }
                throw std::runtime_error(std::string(key.c_str()) + " not found");
        }

        int64_t as_int64(const xml::string& key) const {
                return atoll(get(key));
        }

        double as_double(const xml::string& key) const {
                return atof(get(key));
        }

private:
        const char** attr_;
};

template<class Handler>
class parser : public boost::noncopyable {
public:
        explicit parser(Handler& handler) {
                parser_ = XML_ParserCreate(0);
                if (!parser_)
                        throw std::runtime_error("Could not allocate parser");
                XML_SetUserData(parser_, &handler);
                XML_SetElementHandler(parser_, start_element, end_element);
        }

        ~parser() {
                XML_ParserFree(parser_);
        }

        void parse(std::istream& in) {
                char buf[64*1024];
                while (!in.eof()) {
                        in.read(buf, sizeof (buf));
                        int len = in.gcount();

                        if (!XML_Parse(parser_, buf, len, 0)) {
                                throw parse_error(XML_ErrorString(XML_GetErrorCode(parser_)),
                                                  XML_GetCurrentLineNumber(parser_));
                        }
                }

                XML_Parse(parser_, 0, 0, 1);
        }

        void parse(const std::string& name) {
                std::ifstream file(name.c_str(), std::ios::in | std::ios::binary);
                if (!file.good())
                        throw std::runtime_error("failed to open file " + name);
                if (name.rfind(".gz") == name.size() - 3) {
                        boost::iostreams::filtering_stream<boost::iostreams::input> in;
                        in.push(boost::iostreams::gzip_decompressor());
                        in.push(file);
                        parse(in);
                } else if (name.rfind(".bz2") == name.size() - 4) {
                        boost::iostreams::filtering_stream<boost::iostreams::input> in;
                        in.push(boost::iostreams::bzip2_decompressor());
                        in.push(file);
                        parse(in);
                } else {
                        parse(file);
                }
        }

private:

        XML_Parser parser_;

        static void start_element(void* data, const char* name, const char** attr) {
                static_cast<Handler*>(data)->start_element(string(name), attributes(attr));
        }

        static void end_element(void* data, const char* name) {
                static_cast<Handler*>(data)->end_element(string(name));
        }
};

} // namespace xml

class shape_file : boost::noncopyable {
public:
        explicit shape_file(const std::string& name, bool point)
                : point_(point), shp_(0), dbf_(0) {
                shp_ = SHPCreate(name.c_str(), point ? SHPT_POINT : SHPT_ARC);
                if (!shp_)
                        throw std::runtime_error("Could not open shapefile " + name);
                if (point) {
                        dbf_ = DBFCreate(name.c_str());
                        if (!dbf_) {
                                SHPClose(shp_);
                                throw std::runtime_error("Could not open dbffile " + name);
                        }
                        DBFAddField(dbf_, "name", FTString, 64, 0);
                }
                std::ofstream out((name + ".prj").c_str(), std::ios::out);
                out << "GEOGCS[\"GCS_WGS_1984\",DATUM[\"D_WGS_1984\",SPHEROID[\"WGS_1984\",6378137,298.257223563]],"
                    << "PRIMEM[\"Greenwich\",0],UNIT[\"Degree\",0.017453292519943295]]";
                out.close();
        }

        ~shape_file() {
                SHPClose(shp_);
                if (dbf_)
                        DBFClose(dbf_);
        }

        bool is_point() const {
                return point_;
        }

        void write_point(const std::string& name, double x, double y) {
                assert(point_);
                SHPObject* obj = SHPCreateSimpleObject(SHPT_POINT, 1, &x, &y, 0);
                int id = SHPWriteObject(shp_, -1, obj);
                SHPDestroyObject(obj);
                if (id < 0)
                        throw std::runtime_error("failed to write point object");
                DBFWriteStringAttribute(dbf_, id, 0, name.c_str());
        }

        void write_line(size_t size, const double* x, const double* y) {
                assert(!point_);
                SHPObject* obj = SHPCreateSimpleObject(SHPT_ARC, size,
                                                       const_cast<double*>(x), const_cast<double*>(y), 0);
                int id = SHPWriteObject(shp_, -1, obj);
                SHPDestroyObject(obj);
                if (id < 0)
                        throw std::runtime_error("failed to write line object");
        }

private:
        bool      point_;
        SHPHandle shp_;
        DBFHandle dbf_;
};

namespace osm {

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

template<typename T>
inline bool has_key(const T& map, const typename T::key_type& key) {
        return map.find(key) != map.end();
}

template<typename T, class K, class V>
inline bool has_key_value(const T& map, const K& key, const V& value) {
        typename T::const_iterator i = map.find(key);
        return i != map.end() && i->second == value;
}

struct point_database : boost::noncopyable {
        point_database(const char* name)
                : db_(0), ins_stmt_(0) {
                try {
                        if (sqlite3_open_v2(name, &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0) != SQLITE_OK)
                                db_error("cannot open database");

                        exec("CREATE TABLE IF NOT EXISTS points (id INTEGER NOT NULL PRIMARY KEY,"
                             "x DOUBLE NOT NULL, y DOUBLE NOT NULL)");

                        exec("BEGIN");

                        if (sqlite3_prepare_v2(db_,
                                               "INSERT INTO points (id, x, y) VALUES (?, ?, ?)",
                                               -1, &ins_stmt_, 0) != SQLITE_OK)
                                db_error("cannot prepare statement");
                } catch (...) {
                        close();
                        throw;
                }
        }

        ~point_database() {
                close();
        }

        void set(int64_t id, double x, double y) {
                sqlite3_reset(ins_stmt_);
                sqlite3_clear_bindings(ins_stmt_);
                sqlite3_bind_int64(ins_stmt_, 1, id);
                sqlite3_bind_double(ins_stmt_, 2, x);
                sqlite3_bind_double(ins_stmt_, 3, y);

                int ret = sqlite3_step(ins_stmt_);
                if (ret != SQLITE_DONE && ret != SQLITE_ROW)
                        db_error("step failed");
        }

        bool get(const std::vector<int64_t>& ids, double* xResult, double* yResult) {
                const int block_size = 128;

                int points = ids.size();

                bool resolved[points];
                memset(resolved, 0, points * sizeof (bool));

                stmt_wrapper block_stmt, rest_stmt;

                int todo = points;
                std::vector<int64_t>::const_iterator i = ids.begin();
                while (todo > 0) {
                        int stepSize;
                        sqlite3_stmt* stmt;
                        if (todo > block_size) {
                                stepSize = block_size;
                                if (block_stmt) {
                                        sqlite3_reset(block_stmt);
                                        sqlite3_clear_bindings(block_stmt);
                                } else {
                                        block_stmt.stmt = build_fetch_stmt(block_size);
                                }
                                stmt = block_stmt;
                        } else {
                                stepSize = todo;
                                stmt = rest_stmt.stmt = build_fetch_stmt(todo);
                        }

                        for (int n = 1; n <= stepSize; ++n)
                                sqlite3_bind_int64(stmt, n, *i++);

                        int ret;
                        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
                                sqlite3_int64 found = sqlite3_column_int64(stmt, 0);
                                double x = sqlite3_column_double(stmt, 1);
                                double y = sqlite3_column_double(stmt, 2);
                                int n = 0;
                                foreach (int64_t id, ids) {
                                        if (id == found) {
                                                xResult[n] = x;
                                                yResult[n] = y;
                                                resolved[n] = true;
                                        }
                                        ++n;
                                }
                        }

                        if (ret != SQLITE_DONE)
                                db_error("step failed");

                        todo -= stepSize;
                }

                for (int n = 0; n < points; ++n) {
                        if (!resolved[n]) {
                                std::cerr << "unresolved node " << ids.at(n) << std::endl;
                                return false;
                        }
                }

                return true;
        }

        void clear() {
                exec("DELETE FROM points");
        }

private:

        void close() {
                if (ins_stmt_)
                        sqlite3_finalize(ins_stmt_);
                if (db_) {
                        sqlite3_exec(db_, "COMMIT", 0, 0, 0);
                        sqlite3_close(db_);
                }
        }

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

        sqlite3_stmt* build_fetch_stmt(int how_many) {
                std::string sql = "SELECT id, x, y FROM points WHERE id IN (?";
                for (int i = 1; i < how_many; ++i)
                        sql += ",?";
                sql += ")";

                sqlite3_stmt* stmt;
                if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, 0) != SQLITE_OK)
                        db_error("cannot prepare statement");

                return stmt;
        }

        void db_error(const char* msg) {
                throw std::runtime_error(std::string(msg) + ": " + sqlite3_errmsg(db_));
        }

        void exec(const char* sql) {
                char* error = 0;
                if (sqlite3_exec(db_, sql, 0, 0, &error) != SQLITE_OK) {
                        std::string err(error);
                        sqlite3_free(error);
                        throw std::runtime_error(std::string("invalid sql - ") + sql + " - " + err);
                }
        }

        sqlite3*      db_;
        sqlite3_stmt* ins_stmt_;
};

class handler {
        typedef std::map<std::string, shape_file*> shape_map;
        typedef std::map<std::string, std::string> tag_map;

public:

        handler(const std::string& base)
                : tmp_nodes_(".tmpnodes.sqlite"),
                  processed_nodes_(0), processed_ways_(0),
                  exported_nodes_(0), exported_ways_(0),
                  base_path_(base), taggable_(false) {

                tmp_nodes_.clear();
		mkdir(base.c_str(), 0755);

                add_shape("roadbig_line",    false);
                add_shape("roadmedium_line", false);
                add_shape("roadsmall_line",  false);
                add_shape("citybig_point",   true);
                add_shape("citysmall_point", true);

                add_layer("roadbig_line",    "highway", "motorway");
                add_layer("roadbig_line",    "highway", "trunk");
                add_layer("roadmedium_line", "highway", "primary");
                add_layer("roadsmall_line" , "highway", "secondary");
                add_layer("citybig_point",   "place",   "city");
                add_layer("citysmall_point", "place",   "town");
	}

        ~handler() {
                std::cout << "Total exported nodes: "   << exported_nodes_
                          << "\nTotal exported ways:  " << exported_ways_ << std::endl;

                foreach (shape_map::value_type& value, shapes_)
                        delete value.second;
        }

        void start_element(const xml::string& name, const xml::attributes& attr) {
                if (name == "node")
                        start_node(attr);
                else if (name == "way")
                        start_way(attr);
                else if (name == "nd")
                        start_nd(attr);
                else if (taggable_ && name == "tag")
                        start_tag(attr);
        }

        void end_element(const xml::string& name) {
                if (name == "node")
                        end_node();
                else if (name == "way")
                        end_way();
        }

private:

        void add_shape(const std::string& name, bool point) {
                shapes_[name] = new shape_file(base_path_ + "/" + name, point);
        }

        void add_layer(const std::string& name, const std::string& type, const std::string& subtype) {
                shape_file* shape = shapes_[name];
                assert(shape);
                if (shape->is_point())
                        node_layers_.push_back(layer(shape, type, subtype));
                else
                        way_layers_.push_back(layer(shape, type, subtype));
        }

        void start_node(const xml::attributes& attr) {
                taggable_ = true;
                tags_.clear();
                id_ = attr.as_int64("id");
                x_  = attr.as_double("lon");
                y_  = attr.as_double("lat");
        }

        void start_way(const xml::attributes& attr) {
                taggable_ = true;
                tags_.clear();
                nodes_.clear();
        }

        void start_nd(const xml::attributes& attr) {
                nodes_.push_back(attr.as_int64("ref"));
        }

        void start_tag(const xml::attributes& attr) {
                tags_[attr["k"]] = attr["v"];
        }

        void end_node() {
                taggable_ = false;

                if (++processed_nodes_ % 100000 == 0)
                        std::cout << processed_nodes_ << " nodes processed, " << exported_nodes_ << " nodes exported" << std::endl;

                if (id_ <= 0)
                        return;

                tmp_nodes_.set(id_, x_, y_);

                tag_map::const_iterator i = tags_.find("name");
                if (i == tags_.end())
                        return;

                foreach (const layer& lay, node_layers_) {
                        if (has_key_value(tags_, lay.type(), lay.subtype())) {
                                lay.shape()->write_point(i->second, x_, y_);
                                ++exported_nodes_;
                                break;
                        }
                }
        }

        void end_way() {
                taggable_ = false;

                if (++processed_ways_ % 10000 == 0)
                        std::cout << processed_ways_ << " ways processed, " << exported_ways_ << " ways exported" << std::endl;

                if (is_area() || nodes_.size() < 2)
                        return;

                foreach (const layer& lay, way_layers_) {
                        if (has_key_value(tags_, lay.type(), lay.subtype())) {
                                double x[nodes_.size()], y[nodes_.size()];
                                if (tmp_nodes_.get(nodes_, x, y)) {
                                        lay.shape()->write_line(nodes_.size(), x, y);
                                        ++exported_ways_;
                                }
                                break;
                        }
                }
        }

        bool is_area() {
                return has_key_value(tags_, "area", "yes")               ||
                                has_key(tags_, "landuse")                ||
                                has_key_value(tags_, "natural", "land")  ||
                                has_key_value(tags_, "natural", "water") ||
                                has_key_value(tags_, "natural", "woord");
        }

        point_database       tmp_nodes_;
        int64_t              processed_nodes_;
        int64_t              processed_ways_;
        int64_t              exported_nodes_;
        int64_t              exported_ways_;
        std::vector<layer>   node_layers_;
        std::vector<layer>   way_layers_;
        std::vector<int64_t> nodes_;
        std::string          base_path_;
        bool                 taggable_;
        tag_map              tags_;
        int64_t              id_;
        double               x_, y_;
        shape_map            shapes_;
};

} // namespace osm

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

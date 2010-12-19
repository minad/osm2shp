#include <sqlite3.h>
#include <libshp/shapefil.h>
#include <stdint.h>
#include <expat.h>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <boost/format.hpp>
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

        const char* get(const xml::string& key) const;
        int64_t as_int64(const xml::string& key) const;
        double as_double(const xml::string& key) const;

private:
        const char** attr_;
};

const char* attributes::get(const xml::string& key) const {
        for (int i = 0; attr_[i]; i += 2) {
                if (key == attr_[i])
                        return attr_[i + 1];
        }
        throw std::runtime_error(std::string(key.c_str()) + " not found");
}

int64_t attributes::as_int64(const xml::string& key) const {
        return atoll(get(key));
}

double attributes::as_double(const xml::string& key) const {
        return atof(get(key));
}

template<class Handler>
class parser : public boost::noncopyable {
public:
        explicit parser(Handler& handler);
        ~parser();

        void parse(std::istream& in);
        void parse(const std::string& name);

private:

        static void start_element(void* data, const char* name, const char** attr);
        static void end_element(void* data, const char* name);

        XML_Parser parser_;
};

template<class Handler>
parser<Handler>::parser(Handler& handler)
        : parser_(XML_ParserCreate(0)) {
        if (!parser_)
                throw std::runtime_error("Could not allocate parser");
        XML_SetUserData(parser_, &handler);
        XML_SetElementHandler(parser_, start_element, end_element);
}

template<class Handler>
parser<Handler>::~parser() {
        XML_ParserFree(parser_);
}

template<class Handler>
void parser<Handler>::parse(std::istream& in) {
        char buf[64*1024];
        while (!in.eof()) {
                in.read(buf, sizeof (buf));
                if (!XML_Parse(parser_, buf, in.gcount(), 0)) {
                        throw parse_error(XML_ErrorString(XML_GetErrorCode(parser_)),
                                          XML_GetCurrentLineNumber(parser_));
                }
        }
        XML_Parse(parser_, 0, 0, 1);
}

template<class Handler>
void parser<Handler>::parse(const std::string& name) {
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

template<class Handler>
void parser<Handler>::start_element(void* data, const char* name, const char** attr) {
        static_cast<Handler*>(data)->start_element(string(name), attributes(attr));
}

template<class Handler>
void parser<Handler>::end_element(void* data, const char* name) {
        static_cast<Handler*>(data)->end_element(string(name));
}

} // namespace xml

namespace osm {

class shape_file : boost::noncopyable {
public:
        explicit shape_file(const std::string& name, int type);
        ~shape_file();

        int type() const {
                return type_;
        }

        void add_field(const char* name);
        void add_attribute(int field, const std::string& value);
        void point(double x, double y);
        void multipoint(int type, size_t size, const double* x, const double* y);

private:
        void open_shp();
        void open_dbf();
        void create_prj();

        std::string name_;
        int         type_;
        SHPHandle   shp_;
        DBFHandle   dbf_;
        int         current_id_;
};

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
        point_database(const std::string& name);
        ~point_database();

        const std::string& name() const {
                return name_;
        }

        void set(int64_t id, double x, double y);
        bool get(const std::vector<int64_t>& ids, double* x_result, double* y_result);

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

point_database::point_database(const std::string& name)
        : name_(name), db_(0), ins_stmt_(0) {
        try {
                if (sqlite3_open_v2(name.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0) != SQLITE_OK)
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

point_database::~point_database() {
        close();
        unlink(name_.c_str());
}

void point_database::set(int64_t id, double x, double y) {
        sqlite3_reset(ins_stmt_);
        sqlite3_clear_bindings(ins_stmt_);
        sqlite3_bind_int64(ins_stmt_, 1, id);
        sqlite3_bind_double(ins_stmt_, 2, x);
        sqlite3_bind_double(ins_stmt_, 3, y);

        int ret = sqlite3_step(ins_stmt_);
        if (ret != SQLITE_DONE && ret != SQLITE_ROW)
                db_error("step failed");
}

bool point_database::get(const std::vector<int64_t>& ids, double* x_result, double* y_result) {
        const int block_size = 128;

        int points = ids.size();

        bool resolved[points];
        memset(resolved, 0, points * sizeof (bool));

        stmt_wrapper block_stmt, rest_stmt;

        int todo = points;
        std::vector<int64_t>::const_iterator i = ids.begin();
        while (todo > 0) {
                int step_size;
                sqlite3_stmt* stmt;
                if (todo > block_size) {
                        step_size = block_size;
                        if (block_stmt) {
                                sqlite3_reset(block_stmt);
                                sqlite3_clear_bindings(block_stmt);
                        } else {
                                block_stmt.stmt = build_fetch_stmt(block_size);
                        }
                        stmt = block_stmt;
                } else {
                        step_size = todo;
                        stmt = rest_stmt.stmt = build_fetch_stmt(todo);
                }

                for (int n = 1; n <= step_size; ++n)
                        sqlite3_bind_int64(stmt, n, *i++);

                int ret;
                while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
                        sqlite3_int64 found = sqlite3_column_int64(stmt, 0);
                        double x = sqlite3_column_double(stmt, 1);
                        double y = sqlite3_column_double(stmt, 2);
                        int n = 0;
                        foreach (int64_t id, ids) {
                                if (id == found) {
                                        x_result[n] = x;
                                        y_result[n] = y;
                                        resolved[n] = true;
                                }
                                ++n;
                        }
                }

                if (ret != SQLITE_DONE)
                        db_error("step failed");

                todo -= step_size;
        }

        for (int n = 0; n < points; ++n) {
                if (!resolved[n]) {
                        std::cerr << "unresolved node " << ids.at(n) << std::endl;
                        return false;
                }
        }

        return true;
}

void point_database::close() {
        if (ins_stmt_)
                sqlite3_finalize(ins_stmt_);
        if (db_) {
                sqlite3_exec(db_, "COMMIT", 0, 0, 0);
                sqlite3_close(db_);
        }
}

sqlite3_stmt* point_database::build_fetch_stmt(int how_many) {
        std::string sql = "SELECT id, x, y FROM points WHERE id IN (?";
        for (int i = 1; i < how_many; ++i)
                sql += ",?";
        sql += ")";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, 0) != SQLITE_OK)
                db_error("cannot prepare statement");

        return stmt;
}

void point_database::exec(const char* sql) {
        char* error = 0;
        if (sqlite3_exec(db_, sql, 0, 0, &error) != SQLITE_OK) {
                std::string err(error);
                sqlite3_free(error);
                throw std::runtime_error(std::string("invalid sql - ") + sql + " - " + err);
        }
}

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
        add_shape("citybig_point",    SHPT_POINT);
        add_shape("citymedium_point", SHPT_POINT);
        add_shape("citysmall_point",  SHPT_POINT);
        add_shape("water_line",       SHPT_ARC);
        add_shape("water_area",       SHPT_POLYGON);
        //add_shape("city_area",        SHPT_POLYGON);

        add_layer("roadbig_line",     "highway",  "motorway");
        add_layer("roadbig_line",     "highway",  "trunk");
        add_layer("roadmedium_line",  "highway",  "primary");
        add_layer("roadsmall_line" ,  "highway",  "secondary");
        add_layer("railway_line",     "railway",  "rail");
        add_layer("citybig_point",    "place",    "city");
        add_layer("citymedium_point", "place",    "town");
        add_layer("citymedium_point", "place",    "suburb");
        add_layer("citysmall_point",  "place",    "village");
        add_layer("water_line",       "waterway", "river");
        add_layer("water_line",       "waterway", "canal");
        add_layer("water_area",       "natural",  "water");
        // add_layer("city_area",        "place",    "city");
        // add_layer("city_area",        "place",    "town");
        // add_layer("city_area",        "place",    "suburb");
        // add_layer("city_area",        "place",    "village");
        // add_layer("city_area",        "landuse",  "residential");
        // add_layer("city_area",        "landuse",  "commercial");
        // add_layer("city_area",        "landuse",  "garages");
        // add_layer("city_area",        "landuse",  "industrial");
        // add_layer("city_area",        "landuse",  "retail");
        // add_layer("city_area",        "landuse",  "village_green");
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

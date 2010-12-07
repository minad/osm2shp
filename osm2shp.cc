#include <sqlite3.h>
#include <libshp/shapefil.h>
#include <stdint.h>
#include <expat.h>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <limits>
#include <sys/stat.h>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filtering_stream.hpp>

#define foreach BOOST_FOREACH

class Taggable {
        typedef std::map<std::string, std::string> TagMap;
public:
        virtual ~Taggable() {
        }

        void addTag(const std::string& key, const std::string& value) {
                tags_[key] = value;
        }

        bool hasTag(const std::string& key) const {
                TagMap::const_iterator i = tags_.find(key);
                return i != tags_.end();
        }

        bool hasTagValue(const std::string& key, const std::string& value) const {
                TagMap::const_iterator i = tags_.find(key);
                return i != tags_.end() && i->second == value;
        }

        const std::string& tagValue(const std::string& key) const {
                TagMap::const_iterator i = tags_.find(key);
                static const std::string empty;
                return i == tags_.end() ? empty : i->second;
        }

        virtual void clear() {
                tags_.clear();
        }

private:
        TagMap tags_;
};

struct Way : public Taggable {
        std::vector<int64_t> nodes;

        void clear() {
                Taggable::clear();
                nodes.clear();
        }
};

struct Node : public Taggable {
        int64_t id;
        double  x;
        double  y;

        void clear() {
                Taggable::clear();
                id = -1;
                x = y = std::numeric_limits<double>::max();
        }
};

void error(const std::string& err) {
        throw std::runtime_error(err);
}

class ShapeFile : boost::noncopyable {
public:
        explicit ShapeFile(const std::string& name, bool point)
                : point_(point), shp_(0), dbf_(0) {
                shp_ = SHPCreate(name.c_str(), point ? SHPT_POINT : SHPT_ARC);
                if (!shp_)
                        error("Could not open shapefile " + name);
                if (point) {
                        dbf_ = DBFCreate(name.c_str());
                        if (!dbf_) {
                                SHPClose(shp_);
                                error("Could not open dbffile " + name);
                        }
                        DBFAddField(dbf_, "name", FTString, 256, 0);
                }
                std::ofstream out((name + ".prj").c_str(), std::ios::out);
                out << "GEOGCS[\"GCS_WGS_1984\",DATUM[\"D_WGS_1984\",SPHEROID[\"WGS_1984\",6378137,298.257223563]],"
                    << "PRIMEM[\"Greenwich\",0],UNIT[\"Degree\",0.017453292519943295]]";
                out.close();
        }

        ~ShapeFile() {
                SHPClose(shp_);
                if (dbf_)
                        DBFClose(dbf_);
        }

        bool isPoint() const {
                return point_;
        }

        void writePoint(const std::string& name, double x, double y) {
                SHPObject* obj = SHPCreateSimpleObject(SHPT_POINT, 1, &x, &y, 0);
                int id = SHPWriteObject(shp_, -1, obj);
                SHPDestroyObject(obj);
                if (id < 0)
                        error("failed to write point object");
                DBFWriteStringAttribute(dbf_, id, 0, name.c_str());
        }

        void writeLine(size_t size, const double* x, const double* y) {
                SHPObject* obj = SHPCreateSimpleObject(SHPT_ARC, size,
                                                       const_cast<double*>(x), const_cast<double*>(y), 0);
                int id = SHPWriteObject(shp_, -1, obj);
                SHPDestroyObject(obj);
                if (id < 0)
                        error("failed to write line object");
        }

private:
        bool      point_;
        SHPHandle shp_;
        DBFHandle dbf_;
};

class Layer {
public:
        Layer(ShapeFile* shapeFile, const std::string& type, const std::string& subType)
                : shapeFile_(shapeFile), type_(type), subType_(subType) {
        }

        const std::string& type() const {
                return type_;
        }

        const std::string& subType() const {
                return subType_;
        }

        bool isPoint() const {
                return shapeFile_->isPoint();
        }

        ShapeFile* shapeFile() const {
                return shapeFile_;
        }

private:
        ShapeFile*  shapeFile_;
        std::string type_;
        std::string subType_;
};

struct PointMap : boost::noncopyable {
        PointMap() {
                if (sqlite3_open_v2("pointmap.sqlite", &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0) != SQLITE_OK) {
                        std::string error_str(sqlite3_errmsg(db_));
                        sqlite3_close(db_);
                        error(boost::str(boost::format("cannot open database: %1%") % error_str));
                }
                char* err = 0;
                if (sqlite3_exec(db_, "CREATE TABLE points (id INTEGER NOT NULL PRIMARY KEY, x DOUBLE NOT NULL, y DOUBLE NOT NULL)",
                                 0, 0, &err) != SQLITE_OK) {
                        std::string error_str(err);
                        sqlite3_free(err);
                        sqlite3_close(db_);
                        error(boost::str(boost::format("cannot create table: %1%") % error_str));
                }

                if (sqlite3_exec(db_, "BEGIN", 0, 0, &err) != SQLITE_OK) {
                        std::string error_str(err);
                        sqlite3_free(err);
                        sqlite3_close(db_);
                        error(boost::str(boost::format("cannot begin transaction: %1%") % error_str));
                }

                if (sqlite3_prepare_v2(db_,
                                       "INSERT INTO points (id, x, y) VALUES (?, ?, ?)",
                                       -1, &insStmt_, 0) != SQLITE_OK) {
                        std::string error_str(sqlite3_errmsg(db_));
                        sqlite3_close(db_);
                        error(boost::str(boost::format("cannot prepare statement: %1%") % error_str));
                }
        }

        ~PointMap() {
                char* err = 0;
                sqlite3_exec(db_, "COMMIT", 0, 0, &err);
                sqlite3_finalize(insStmt_);
                sqlite3_close(db_);
                unlink("pointmap.sqlite");
        }

        void set(int64_t id, double x, double y) {
                sqlite3_reset(insStmt_);
                sqlite3_clear_bindings(insStmt_);
                sqlite3_bind_int64(insStmt_, 1, id);
                sqlite3_bind_double(insStmt_, 2, x);
                sqlite3_bind_double(insStmt_, 3, y);

                int ret = sqlite3_step(insStmt_);
                if (ret != SQLITE_DONE && ret != SQLITE_ROW)
                        std::cerr << "sqlite3_step() failed" << std::endl;
        }

        bool get(const std::vector<int64_t>& ids, double* xResult, double* yResult) {
                const int blockSize = 128;

                int points = ids.size();

                bool resolved[points];
                memset(resolved, 0, points * sizeof (bool));

                sqlite3_stmt *blockStmt = 0, *restStmt = 0;

                int todo = points;
                std::vector<int64_t>::const_iterator i = ids.begin();
                while (todo > 0) {
                        int stepSize;
                        sqlite3_stmt* stmt;
                        if (todo > blockSize) {
                                stepSize = blockSize;
                                if (blockStmt) {
                                        sqlite3_reset(blockStmt);
                                        sqlite3_clear_bindings(blockStmt);
                                } else {
                                        blockStmt = buildFetchStmt(blockSize);
                                }
                                stmt = blockStmt;
                        } else {
                                stepSize = todo;
                                stmt = restStmt = buildFetchStmt(todo);
                        }

                        if (!stmt) {
                                if (blockStmt)
                                        sqlite3_finalize(blockStmt);
                                if (restStmt)
                                        sqlite3_finalize(restStmt);
                                return false;
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

                        if (ret != SQLITE_DONE) {
                                std::cerr << "sqlite3_step() error: " << sqlite3_errmsg(db_) << std::endl;
                                if (blockStmt)
                                        sqlite3_finalize(blockStmt);
                                if (restStmt)
                                        sqlite3_finalize(restStmt);
                                return false;
                        }

                        todo -= stepSize;
                }

                if (blockStmt)
                        sqlite3_finalize(blockStmt);
                if (restStmt)
                        sqlite3_finalize(restStmt);

                for (int n = 0; n < points; ++n) {
                        if (!resolved[n]) {
                                std::cerr << "unresolved node " << ids.at(n) << std::endl;
                                return false;
                        }
                }

                return true;
        }

private:

        sqlite3_stmt* buildFetchStmt(int how_many) {
                std::string sql = "SELECT id, x, y FROM points WHERE id IN (?";
                for (int i = 1; i < how_many; ++i)
                        sql += ",?";
                sql += ")";

                sqlite3_stmt* stmt;
                if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
                        std::cerr << "cannot prepare statement " << sqlite3_errmsg(db_) << std::endl;
                        return 0;
                }

                return stmt;
        }

        sqlite3* db_;
        sqlite3_stmt* insStmt_;
};

struct State {
        Way                way;
        Node               node;
        PointMap           tmpNodes;
        int64_t            processedNodes;
        int64_t            processedWays;
        int64_t            insertedNodes;
        int64_t            insertedWays;
        Taggable*          taggable;
        std::vector<Layer> layers;
        std::map<std::string, boost::shared_ptr<ShapeFile> > shapeFiles;
        std::string        basePath;

        State(const std::string& base)
                : processedNodes(0), processedWays(0),
                  insertedNodes(0), insertedWays(0), taggable(0), basePath(base) {
		mkdir(base.c_str(), 0755);
	}

        void addShapeFile(const std::string& name, bool point) {
                shapeFiles[name] = boost::shared_ptr<ShapeFile>(new ShapeFile(basePath + "/" + name, point));
        }

        void addLayer(const std::string& name, const std::string& type, const std::string& subType) {
                layers.push_back(Layer(shapeFiles[name].get(), type, subType));
        }
};

void startNode(State* state, const char** attr) {
        state->taggable = &state->node;

        Node& node = state->node;
        node.clear();

        for (int i = 0; attr[i]; i += 2) {
                if (!strcmp(attr[i], "id"))
                        node.id = atoll(attr[i + 1]);
                else if (!strcmp(attr[i], "lat"))
                        node.y = atof(attr[i + 1]);
                else if (!strcmp(attr[i], "lon"))
                        node.x = atof(attr[i + 1]);
        }
}

void startWay(State* state, const char** attr) {
        state->taggable = &state->way;
        state->way.clear();
}

void startNd(State* state, const char** attr) {
        for (int i = 0; attr[i]; i += 2) {
                if (!strcmp(attr[i], "ref")) {
                        state->way.nodes.push_back(atoll(attr[i+1]));
                        break;
                }
        }
}

void startTag(State* state, const char** attr) {
        const char *key = 0, *value = 0;
        for (int i = 0; attr[i]; i += 2) {
                if (!strcmp(attr[i], "k"))
                        key = attr[i + 1];
                else if (!strcmp(attr[i], "v"))
                        value = attr[i + 1];
                if (key && value && state->taggable) {
                        state->taggable->addTag(key, value);
                        break;
                }
        }
}

void startElement(void* data, const char* name, const char** attr) {
        State* state = static_cast<State*>(data);
        if (!strcmp(name, "node"))
                startNode(state, attr);
        else if (!strcmp(name, "tag"))
                startTag(state, attr);
        else if (!strcmp(name, "way"))
                startWay(state, attr);
        else if (!strcmp(name, "nd"))
                startNd(state, attr);
}

void endNode(State* state) {
        state->taggable = 0;

        if (++state->processedNodes % 100000 == 0)
                std::cout << state->processedNodes << " nodes processed" << std::endl;

        Node& node = state->node;

        if (node.id <= 0)
                return;

        state->tmpNodes.set(node.id, node.x, node.y);

        const std::string& name = node.tagValue("name");
        if (name.empty())
                return;

        foreach (const Layer& layer, state->layers) {
                if (!layer.isPoint())
                        continue;
                if (node.hasTagValue(layer.type(), layer.subType())) {
                        layer.shapeFile()->writePoint(name, node.x, node.y);
                        ++state->insertedNodes;
                        break;
                }
        }
}

bool isArea(const Taggable& tags) {
        return tags.hasTagValue("area", "yes")               ||
                        tags.hasTag("landuse")               ||
                        tags.hasTagValue("natural", "land")  ||
                        tags.hasTagValue("natural", "water") ||
                        tags.hasTagValue("natural", "woord");
}

void endWay(State* state) {
        state->taggable = 0;

        if (++state->processedWays % 10000 == 0)
                std::cout << state->processedWays << " ways processed" << std::endl;

        Way& way = state->way;
        if (isArea(way) || way.nodes.size() < 2)
                return;

        foreach (const Layer& layer, state->layers) {
                if (layer.isPoint())
                        continue;
                if (way.hasTagValue(layer.type(), layer.subType())) {
                        double x[way.nodes.size()], y[way.nodes.size()];
                        if (state->tmpNodes.get(way.nodes, x, y)) {
                                layer.shapeFile()->writeLine(way.nodes.size(), x, y);
                                ++state->insertedWays;
                        }
                        break;
                }
        }
}

void endElement(void* data, const char* name) {
        State* state = static_cast<State*>(data);
        if (!strcmp(name, "node"))
                endNode(state);
        else if (!strcmp(name, "way"))
                endWay(state);
}

void configure(State& state) {
        state.addShapeFile("roadbig_line",    false);
        state.addShapeFile("roadmedium_line", false);
        state.addShapeFile("roadsmall_line",  false);
        state.addShapeFile("citybig_point",   true);
        state.addShapeFile("citysmall_point", true);

        state.addLayer("roadbig_line",    "highway", "motorway");
        state.addLayer("roadbig_line",    "highway", "trunk");
        state.addLayer("roadmedium_line", "highway", "primary");
        state.addLayer("roadsmall_line" , "highway", "secondary");
        state.addLayer("citybig_point",   "place",   "city");
        state.addLayer("citysmall_point", "place",   "town");
}

void parseStream(std::istream& in, const std::string& base) {
        XML_Parser parser = XML_ParserCreate(0);
        if (!parser)
                error("Could not allocate parser");
	XML_SetElementHandler(parser, startElement, endElement);

        State state(base);
        configure(state);
        XML_SetUserData(parser, &state);

        char buf[64*1024];
	while (!in.eof()) {
                in.read(buf, sizeof (buf));
                int len = in.gcount();

		if (!XML_Parse(parser, buf, len, 0)) {
                        error(boost::str(boost::format("Parse error at line %1%: %2%") %
                                         XML_GetCurrentLineNumber(parser) %
                                         XML_ErrorString(XML_GetErrorCode(parser))));
                }
        }

        XML_Parse(parser, 0, 0, 1);
        XML_ParserFree(parser);

        std::cout << "Exported nodes: "   << state.insertedNodes
                  << "\nExported ways:  " << state.insertedWays << std::endl;
}

void parseFile(const std::string& name, const std::string& base) {
        std::ifstream file(name.c_str(), std::ios::in | std::ios::binary);
        if (!file.good())
                error("failed to open file " + name);
        if (name.rfind(".gz") == name.size() - 3) {
                boost::iostreams::filtering_stream<boost::iostreams::input> in;
                in.push(boost::iostreams::gzip_decompressor());
                in.push(file);
                parseStream(in, base);
        } else if (name.rfind(".bz2") == name.size() - 4) {
                boost::iostreams::filtering_stream<boost::iostreams::input> in;
                in.push(boost::iostreams::bzip2_decompressor());
                in.push(file);
                parseStream(in, base);
        } else {
                parseStream(file, base);
        }
}

int main(int argc, char* argv[]) {
        try {
                if (argc != 3) {
                        std::cerr << "usage: " << argv[0] << " planet.osm(.gz|.bz2) base-path" << std::endl;
                        return 1;
                }
                parseFile(argv[1], argv[2]);
                return 0;
        } catch (const std::exception& ex) {
                std::cerr << ex.what() << std::endl;
                return 1;
        }
}

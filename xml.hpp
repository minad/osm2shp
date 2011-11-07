#ifndef OSM2SHP_XML_HPP
#define OSM2SHP_XML_HPP

#include <expat.h>
#include <iostream>
#include <fstream>
#include <boost/format.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filtering_stream.hpp>

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

#endif

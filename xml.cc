#include "xml.hpp"

namespace xml {

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

} // namespace xml

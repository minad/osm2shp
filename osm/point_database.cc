#include "point_database.hpp"

#include <boost/foreach.hpp>
#include <iostream>

#define foreach BOOST_FOREACH

namespace osm {

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

}

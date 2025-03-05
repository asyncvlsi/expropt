/*************************************************************************
 *
 *  Copyright (c) 2025 Karthi Srinivasan
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 **************************************************************************
 */
#pragma once

#include <act/expropt.h>

/*
    Path of an expression file within the cache.
    int -> uses an integer as filename.
*/
typedef int expr_path;

static const std::string _tmp_expr_file = "tmp_expr.act";
static const std::string _cache_dummy_ns = "_cache_ns_";

class ExprCache : public ExternalExprOpt {
public:

    ExprCache(  const char *datapath_synthesis_tool,
                const expr_mapping_target mapping_target,
                const bool tie_cells,
                const std::string expr_file_path = "");
    
    ~ExprCache();

    /*
        Top-level function - This is what you would call instead of 
        run_external_opt for the expropt object. 
        Arguments are exactly the same.
    */
    ExprBlockInfo *synth_expr (int, int, Expr *, list_t *, iHashtable *, iHashtable *);

    /*
        Get path to cache that is being used.
    */
    std::string get_cache_loc ();

private:

    void read_cache ();
    void read_cache_index_line (std::string);
    void write_cache_index_line (std::string);
    void rename_and_pipe (std::ifstream &, std::ofstream &, const std::string, const std::string);

    int lock_file (std::string);
    void unlock_file (int);

    std::string _gen_unique_id (Expr *, list_t *, iHashtable *);

    /*
        define a next() function for the
        expr_path type.
    */
    expr_path gen_expr_path () {
        return cache_counter++;
    }

    std::string path;
    std::string index_file;

    char idx_file_delimiter;
    int n_metrics;
    int n_cols;
    int area_id;
    int mapper_runtime_id;
    int io_runtime_id;

    std::string _expr_file_path;

    expr_path cache_counter;

    // ID-to-path
    std::unordered_map<std::string, expr_path> path_map;
    // Path-to-info
    std::unordered_map<expr_path, ExprBlockInfo> info_map;

};
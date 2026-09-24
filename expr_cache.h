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
// #include "expropt.h"

/*
    Path of an expression file within the cache.
    int -> uses an integer as filename.
*/
typedef int expr_path;

static const std::string _tmp_expr_file = "tmp_expr.act";

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
    ExprBlockInfo *synth_expr (int, Expr *, list_t *, iHashtable *, iHashtable *);

    /*
        Get path to cache that is being used.
    */
    std::string get_cache_loc ();

    void set_expr_outfile(std::string x) {
        expr_output_file = x;
    }

private:
    std::string _gen_unique_id (Expr *, iHashtable *, iHashtable *, int);
    std::string path;
    std::string _expr_file_path;
    std::unordered_set<std::string> runtime_accessed_set;
};

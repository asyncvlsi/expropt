/*************************************************************************
 *
 *  This file is part of act expropt
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

#include "expropt.h"

typedef int expr_path;

enum class MetricID {Delay};

static const std::string _tmp_expr_file = "tmp_expr.act";

class ExprCache : public ExternalExprOpt {
public:

    ExprCache( bool invalidate_cache, 
                const char *datapath_synthesis_tool,
                const expr_mapping_target mapping_target,
                const bool tie_cells,
                const std::string expr_file_path = "");

    ExprBlockInfo *synth_expr (int, int, Expr *, list_t *, iHashtable *, iHashtable *);

    char *get_cache_loc ();

private:

    void read_cache ();
    void read_cache_index_line (std::string);
    void write_cache_index_line (std::string);

    std::string _gen_unique_id (Expr *, list_t *, iHashtable *);

    // gotta change this for different folder name formats
    expr_path gen_expr_path () {
        return cache_counter++;
    }

    char *path;
    char *index_file;
    char idx_file_delimiter;
    int n_metrics;
    int n_cols;
    int area_id;
    int mapper_runtime_id;
    int io_runtime_id;

    std::string _expr_file_path;

    int cache_counter;

    // ID-to-path
    std::unordered_map<std::string, expr_path> path_map;
    // Path-to-info
    std::unordered_map<expr_path, ExprBlockInfo> info_map;

};
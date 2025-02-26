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

#include <unordered_map>
#include <string>
#include <act/act.h>
#include "expr_info.h"

typedef long long expr_path;

class ExprCache {
public:

    ExprCache ();
    void read_cache ();
    void add_expr_to_cache (Expr *);
    bool expr_in_cache (Expr *);
    ExprBlockInfo get_expr_info (Expr *);

private:
    char *path;
    char *index_file;
    char idx_file_delimiter;
    std::unordered_map<std::string, expr_path> path_map;

};
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

#include <stdio.h>
#include <iostream>
#include <sstream>
#include "expr_cache.h"

#include <filesystem>
namespace fs = std::filesystem;

expr_path to_expr_path (std::string x) {
    return std::stoi(x);
}

char *ExprCache::get_cache_loc()
{
    Assert (getenv("ACT_SYNTH_CACHE"), "Could not find cache location");
    return getenv("ACT_SYNTH_CACHE");
}

ExprCache::ExprCache(const char *datapath_syntesis_tool,
                     const expr_mapping_target mapping_target,
                     const bool tie_cells,
                     const std::string expr_file_path,
                     const std::string exprid_prefix, 
                     const std::string block_prefix)
 :
    ExternalExprOpt ( datapath_syntesis_tool,
                      mapping_target,
                      tie_cells,
                      expr_file_path,
                      exprid_prefix,
                      block_prefix) 
{
    if (getenv("ACT_SYNTH_CACHE")==NULL) {
        setenv("ACT_SYNTH_CACHE", (std::string(getenv("ACT_HOME"))+std::string("/cache")).c_str(), 1);
    }
    path = getenv("ACT_SYNTH_CACHE");
    
    fs::path cache_path = path;
    if (!fs::exists(cache_path)) {
        if (!(fs::create_directories(cache_path))) {
            std::cerr << "Could not create directory" << cache_path << std::endl;
            exit(1);
        }
    }

    std::string index_filename = std::string(path) + std::string("/expr.index");
    if (!fs::exists(index_filename)) {
        std::ofstream idx_file (index_filename.c_str(), std::ios::app);
        if (!idx_file) {
            std::cerr << "Error: could not create/open " << index_filename << std::endl;
            exit(1);
        }
        idx_file << "# ------------------------------------------------------------------------------------------------------------------------" << std::endl;
        idx_file << "# Expression cache index and metrics file" << std::endl;
        idx_file << "# Metrics except area are in triplets (min,typ,max)" << std::endl;
        idx_file << "# Format: <unique_id> <dir_name> <delay> <static power> <dynamic power> <total power> <area> <mapper_runtime> <io_runtime>" << std::endl;
        idx_file << "# Type: <string> <int> <double (s)> <double (W)> <double (W)> <double (W)> <double (W)> <mapper_runtime (us)> <io_runtime (us)>" << std::endl;
        idx_file << "# ------------------------------------------------------------------------------------------------------------------------" << std::endl;
        idx_file.close();
    }
    index_file = Strdup(index_filename.c_str());
    idx_file_delimiter = ' ';
    path_map.clear();

    // the number of metrics to store
    n_metrics = 4;
    area_id = 2 + (3*n_metrics);
    mapper_runtime_id = 2 + (3*n_metrics) + 1;
    io_runtime_id = 2 + (3*n_metrics) + 2;

    // id, dirname, 3 numbers for each metric (typ, min, max), area, mapper runtime, io runtime
    n_cols = 2 + (3*n_metrics) + 1 + 2;

    // initialize cache counter
    cache_counter = 0;
}

std::string ExprCache::_gen_unique_id (Expr *e, list_t *in_expr_list, iHashtable *width_map)
{
    list_t *vars = list_new();
    act_expr_collect_ids (vars, e);
    std::string uniq_id = act_expr_to_string(vars, e);
    for (listitem_t *li = list_first(in_expr_list); li; li = li->next) {
        Expr *evar = (Expr *)(list_value(li));
        auto b = ihash_lookup(width_map, (long)evar);
        Assert (b, "var. width not found");
        int width = b->i;
        // gotta append bitwidth   
        uniq_id.append("_");
        uniq_id.append(std::to_string(width));
    }
    return uniq_id;
}

void ExprCache::add_expr_to_cache (Expr *e, list_t *in_expr_list, iHashtable *width_map, ExprBlockInfo *eb)
{
    std::string uniq_id = _gen_unique_id(e, in_expr_list, width_map);
    if (path_map.contains(uniq_id)) {
        return;
    }

    path_map.insert({uniq_id, gen_expr_path()});
    info_map.insert({path_map[uniq_id],*eb});
    write_cache_index_line (uniq_id);



}

void ExprCache::read_cache()
{
    std::ifstream idx_file(index_file);
    if (!idx_file.is_open()) {
        std::cerr << "Error: Could not open cache index file (" << index_file << ") for reading.\n";
        exit(1);
    }

    std::string line;
    while (std::getline(idx_file, line)) {
        std::istringstream ss(line);
        if (line.at(0)=='#') {
            continue; // comment
        }
        read_cache_index_line(line);
        cache_counter++;
    }
}

void ExprCache::read_cache_index_line (std::string line) {
    std::istringstream ss(line);

    std::vector<std::string> tokens;
    std::string token;
    
    while (std::getline(ss, token, idx_file_delimiter)) {
        tokens.push_back(token);
    }
    
    Assert (tokens.size()==n_cols, "Malformed index file");

    // if (!(fs::exists(tokens[1]) && fs::is_directory(tokens[1]))) {
    //     std::cerr << "Error: cache directory does not exist for " << tokens[1] << std::endl;
    //     exit(1);
    // }

    expr_path loc = to_expr_path(tokens[1]);
    Assert (!path_map.contains(tokens[0]), "duplicate expression in cache index");
    path_map.insert({tokens[0],loc});

    std::vector<metric_triplet> tmp = {};
    for (int i=0; i<n_metrics; i++) {
        metric_triplet mt;
        // min, typ, max in order
        mt.set_metrics(std::stod(tokens[2+(i)]), std::stod(tokens[2+(i+1)]), std::stod(tokens[2+(i+2)]));
        tmp.push_back(mt);
    } 
    Assert (tmp.size()==n_metrics, "Incomplete metrics");

    metric_triplet del = tmp[0];
    metric_triplet pow = tmp[1];
    metric_triplet st_pow = tmp[2];
    metric_triplet dyn_pow = tmp[3];

    double area = std::stod(tokens[area_id]);
    double mapper_runtime = std::stod(tokens[mapper_runtime_id]);
    double io_runtime = std::stod(tokens[io_runtime_id]);

    ExprBlockInfo eb (del, pow, st_pow, dyn_pow, area, mapper_runtime, io_runtime);
    Assert (!info_map.contains(loc), "duplicate data in cache index file");
    info_map.insert({loc, eb});
}

void ExprCache::write_cache_index_line (std::string uniq_id)
{
    std::ofstream idx_file (index_file, std::ios::app);

    Assert (path_map.contains(uniq_id), "Expr not in cache");
    expr_path ep = path_map.at(uniq_id);
    Assert (info_map.contains(ep), "Expr block info not found");
    ExprBlockInfo eb = info_map.at(ep);

    idx_file << uniq_id << idx_file_delimiter << ep << idx_file_delimiter;
    idx_file << eb.getDelay().min_val << idx_file_delimiter << eb.getDelay().typ_val << idx_file_delimiter << eb.getDelay().max_val << idx_file_delimiter;
    idx_file << eb.getStaticPower().min_val << idx_file_delimiter << eb.getStaticPower().typ_val << idx_file_delimiter << eb.getStaticPower().max_val << idx_file_delimiter;
    idx_file << eb.getDynamicPower().min_val << idx_file_delimiter << eb.getDynamicPower().typ_val << idx_file_delimiter << eb.getDynamicPower().max_val << idx_file_delimiter;
    idx_file << eb.getPower().min_val << idx_file_delimiter << eb.getPower().typ_val << idx_file_delimiter << eb.getPower().max_val << idx_file_delimiter;
    idx_file << eb.getArea() << idx_file_delimiter;
    idx_file << eb.getRuntime() << idx_file_delimiter;
    idx_file << eb.getIORuntime();

    idx_file << std::endl;

    idx_file.close();
}
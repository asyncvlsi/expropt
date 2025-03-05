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

#include <stdio.h>
#include <iostream>
#include <sstream>
#include "expr_cache.h"
#include <sys/file.h>   
#include <fcntl.h>    
#include <unistd.h>    

#include <filesystem>
namespace fs = std::filesystem;

expr_path to_expr_path (std::string x) {
    return std::stoi(x);
}

std::string ExprCache::get_cache_loc()
{
    if (config_exists("synth.expropt.cache.local")) {
        return config_get_string ("synth.expropt.cache.local");
    }
    Assert (config_exists("synth.expropt.cache.global"), "Could not find global cache");
    return config_get_string ("synth.expropt.cache.global");
}

ExprCache::~ExprCache()
{}

int ExprCache::lock_file (std::string fn)
{
    int fd = open(fn.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd == -1) { 
        std::cerr << "Failed to open " << fn << "\n"; 
        exit(1); 
    }
    if (flock(fd, LOCK_EX) == -1) { 
        std::cerr << "Failed to lock " << fn << "\n"; 
        close(fd); 
        exit(1); 
    }
    return fd;
}

void ExprCache::unlock_file (int fd)
{
    bool fail = false;
    if (flock(fd, LOCK_UN) == -1) { 
        std::cerr << "Failed to unlock descriptor: " << fd << "\n"; 
        fail = true;
    }
    close(fd);
    if (fail) {
        exit(1);
    }
}

ExprCache::ExprCache(const char *datapath_synthesis_tool,
                     const expr_mapping_target mapping_target,
                     const bool tie_cells,
                     const std::string expr_file_path
                     )
 :  ExternalExprOpt ( datapath_synthesis_tool,
                      mapping_target,
                      tie_cells,
                      _tmp_expr_file,
                      "in_",
                      "blk_") 
{
    _expr_file_path = expr_file_path;
    path = get_cache_loc();

    bool invalidate_cache = false;
    if (config_exists("synth.expropt.cache.invalidate")) {
        invalidate_cache = (config_get_int("synth.expropt.cache.invalidate") != 0);
    }

    config_set_default_string("synth.expropt.cache.cell_lib_namespace", "syn"); 

    if (invalidate_cache) {
        Assert(!(path.empty()), "what");
        std::string del_files_cmd = std::string("rm ") + std::string(path) + std::string("/*.act");
        std::string del_index_cmd = std::string("rm ") + std::string(path) + std::string("/expr.index");
        system(del_files_cmd.c_str());
        system(del_index_cmd.c_str());
    }

    fs::path cache_path = path;
    if (!fs::exists(cache_path)) {
        if (!(fs::create_directories(cache_path))) {
            std::cerr << "Could not create directory" << cache_path << std::endl;
            exit(1);
        }
    }

    std::string index_filename = path + std::string("/expr.index");
    if (!fs::exists(index_filename)) {

        int fd = lock_file(index_filename);

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

        unlock_file(fd);
    }
    index_file = index_filename;
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
    read_cache();
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

ExprBlockInfo *ExprCache::synth_expr (int expr_set_number,
                                      int targetwidth,
                                      Expr *expr,
                                      list_t *in_expr_list,
                                      iHashtable *in_expr_map,
                                      iHashtable *in_width_map)
{
    std::string uniq_id = _gen_unique_id(expr, in_expr_list, in_width_map);

    // already have it
    if (path_map.contains(uniq_id)) {
        auto idx = path_map.at(uniq_id);
        Assert (info_map.contains(idx), "Could not find path to cached process.");
    }
    // gotta synth and add to cache
    else {
        // set namespace to cache namespace
        set_namespace( config_get_string("synth.expropt.cache.cell_lib_namespace") );
        ExprBlockInfo *ebi = run_external_opt(expr_set_number, targetwidth, expr, in_expr_list, in_expr_map, in_width_map);
        reset_namespace();

        auto idx = gen_expr_path();
        path_map.insert({uniq_id, idx});
        Assert (!info_map.contains(idx), "cache identifier conflict");
        info_map.insert({idx, *ebi});

        // save the defproc into the cache file
        Assert (fs::exists(path), "what");
        std::string fn = path;
        fn.append("/");
        fn.append(std::to_string(idx));
        fn.append(".act");
        Assert (!fs::exists(fn), "cache file already exists?");

        // append all contents of tmp expr file to cache file
        std::ifstream sourceFile(_tmp_expr_file);
        if (!sourceFile.is_open()) {
            std::cerr << "Error opening source file: " << _tmp_expr_file << "\n";
            exit(1);
        }
        int fd = lock_file(fn);
        std::ofstream destFile(fn);
        if (!destFile.is_open()) {
            std::cerr << "Error opening dest file: " << fn << "\n";
            exit(1);
        }

        rename_and_pipe(sourceFile, destFile, config_get_string("synth.expropt.cache.cell_lib_namespace"), _cache_dummy_ns);

        if (!fs::remove(_tmp_expr_file)) {
            std::cerr << _tmp_expr_file << " could not be deleted\n";
            exit(1);
        }
        unlock_file(fd);

        write_cache_index_line (uniq_id);
    }

    // read the cached defproc
    Assert (fs::exists(path), "what");
    std::string fn = path;
    fn.append("/");
    fn.append(std::to_string(path_map.at(uniq_id)));
    fn.append(".act");

    // append all contents of reqd. cache file to output expr file
    int fd = lock_file(fn);
    std::ifstream sourceFile(fn);
    if (!sourceFile.is_open()) {
        std::cerr << "Error opening source file: " << fn << "\n";
        exit(1);
    }
    std::ofstream destFile(_expr_file_path, std::ios::app);
    if (!destFile.is_open()) {
        std::cerr << "Error opening dest file: " << _expr_file_path << "\n";
        exit(1);
    }

    rename_and_pipe(sourceFile, destFile, _cache_dummy_ns, cell_namespace);
    unlock_file(fd);

    ExprBlockInfo eb = info_map.at(path_map.at(uniq_id));
    ExprBlockInfo *ebi = new ExprBlockInfo(eb);
    return ebi;
}

void ExprCache::rename_and_pipe (std::ifstream &src, 
                                 std::ofstream &dst,
                                 const std::string sfind,
                                 const std::string sreplace)
{
    std::string line;
    while (std::getline(src, line)) 
    {
        std::size_t pos = 0;
        while ((pos = line.find(sfind, pos)) != std::string::npos) 
        {
            line.replace(pos, sfind.size(), sreplace);
            pos += sreplace.size();
        }
        dst << line << "\n";
    }
}

void ExprCache::read_cache()
{
    int fd = lock_file(index_file);
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
    unlock_file(fd);
}

void ExprCache::read_cache_index_line (std::string line) {
    std::istringstream ss(line);

    std::vector<std::string> tokens;
    std::string token;
    
    while (std::getline(ss, token, idx_file_delimiter)) {
        tokens.push_back(token);
    }
    
    Assert (tokens.size()==n_cols, "Malformed index file");

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
    int fd = lock_file(index_file);
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
    unlock_file(fd);
}
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
#include <fstream>
#include <sstream>
#include "expr_cache.h"
#include <filesystem>
namespace fs = std::filesystem;

ExprCache::ExprCache()
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
    std::ofstream idx_file (index_filename.c_str(), std::ios::app);
    if (!idx_file) {
        std::cerr << "Error: could not create/open " << index_filename << std::endl;
        exit(1);
    }
    idx_file.close();
    index_file = Strdup(index_filename.c_str());
    idx_file_delimiter = ',';
    path_map.clear();
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
        std::cout << line << "\n";
        std::istringstream ss(line);

        std::vector<std::string> tokens;
        std::string token;
        
        while (std::getline(ss, token, idx_file_delimiter)) {
            tokens.push_back(token);
        }
        // Assert (tokens.size()==2, "Malformed index file");
        

    }
}

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

#include <act/types.h>
#include <common/int.h>
#include <string.h>
#include <dlfcn.h>

#include <unordered_map>
#include <string>
#include <act/act.h>

#include <chrono>
using namespace std::chrono;

/*
 * enum for referencing the pass type
 */
enum expr_mapping_target {
        qdi = 0,
        bd = 1,
};

/*
 * the metadata object holds all extracted points of the expr set.
 */

struct metric_triplet {
  metric_triplet() {
    typ_val = 0;
    min_val = 0;
    max_val = 0;
    found = false;
  }
  
/* Metrics typically have a range: typical value, min value, and max
 * value.
 */
  double typ_val, min_val, max_val;
  bool found;
  
  bool exists() { return found; }

  void set_typ_only(double v) {
    min_val = v;
    typ_val = v;
    max_val = v;
    found = true;
  }
  void set_metrics(double v_min, double v_typ, double v_max) {
    min_val = v_min;
    typ_val = v_typ;
    max_val = v_max;
    found = true;
  }
};

enum expropt_metadata {
  metadata_area = 0,
  metadata_delay_typ = 1,
  metadata_power_typ = 2,
  metadata_delay_max = 3,
  metadata_delay_min = 4,
  metadata_power_typ_static = 5,
  metadata_power_typ_dynamic = 6,
  metadata_power_max = 7,
  metadata_power_max_static = 8,
  metadata_power_max_dynamic = 9
};

struct act_syn_info {
  const char *v_in;
  const char *v_out;
  const char *toplevel;
  bool use_tie_cells;
  void *space;			// use for whatever you want!
};

class ExprBlockInfo {
private:
    metric_triplet delay;		//< delay value (s)
    metric_triplet static_power;	//< power value (W)
    metric_triplet dynamic_power; //<  power value (W)
    metric_triplet total_power;	//<  total power (W)
    long long mapper_runtime; //<  logic synthesis tool runtime (us)
    long long interface_runtime; //< interface tools (verilog_printing + v2act) runtime (us)
    std::string mapped_file; //< mapped verilog file
    std::string unique_id; //< unique id for the generated expression (cache only)

    /*
    * the theoretical area of all gates combined, with 100% utiliasation.
    * 0 if not extracted.
    */
    double area;

public:

    metric_triplet getDelay() { return delay; }
    metric_triplet getStaticPower() { return static_power; }
    metric_triplet getDynamicPower() { return dynamic_power; }
    metric_triplet getPower() { return total_power; }
    double getArea() { return area; }
    long long getRuntime() { return mapper_runtime; }
    long long getIORuntime() { return interface_runtime; }
    std::string getMappedFile() { return mapped_file; }
    std::string getID() { return unique_id; }
    void setID(std::string s) { unique_id = s; }

    /**
     * Construct a new Expr Block Info object, 
     * values can not be changed after creation.
     * See getter functions for values.
     */
    ExprBlockInfo(const metric_triplet e_delay,
            const metric_triplet e_power,
            const metric_triplet e_static_power,
            const metric_triplet e_dynamic_power,
            const double e_area,
        const long long e_runtime,
        const long long e_io_runtime,
        std::string e_mapped_file,
        std::string e_unique_id) :
        delay{e_delay},
        total_power{e_power},
        static_power{e_static_power},
        dynamic_power{e_dynamic_power},
        area{e_area},
        mapper_runtime{e_runtime},
        interface_runtime{e_io_runtime},
        mapped_file{e_mapped_file},
        unique_id{e_unique_id}
    { }
                        
    /**
     * Construct a new Expr Block dummy with no extraction results =>
     * all 0, but area = -1 to indicate that the results were not
     * created.
     */
    ExprBlockInfo() : area{-1} { };
    
    ~ExprBlockInfo() { }

    bool exists() { return (area != -1); }
};

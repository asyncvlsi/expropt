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
#include "abc_api.h"
#include "expr_cache.h"
#include <sys/file.h>   
#include <fcntl.h>    
#include <unistd.h>
#include <sqlite3.h>
#include <zlib.h>

#include <filesystem>
namespace fs = std::filesystem;

expr_path to_expr_path (std::string x) {
    return std::stoi(x);
}

std::string ExprCache::get_cache_loc()
{
    std::string ret = "";
    if (config_exists("synth.expropt.cache.local")) {
        ret = config_get_string ("synth.expropt.cache.local");
    }
    else if (config_exists("synth.expropt.cache.global")) {
        ret = config_get_string ("synth.expropt.cache.global");
    }
    else {
        fatal_error ("Could not find local or global expression cache!");
    }

    std::string techname = getenv("ACT_TECH");
    ret.append("/"+techname);

    if (mapper == "abc") {
        ret.append("/abc.db");
    }
    else if (mapper == "yosys") {
        ret.append("/yosys.db");
    }
    else if (mapper == "genus") {
        ret.append("/genus.db");
    }
    else {
        fatal_error ("Unsupported logic synthesis system!");
    }

    return ret;
}

ExprCache::~ExprCache()
{
  if (_syn_dlib) {
    dlclose (_syn_dlib);
    _syn_dlib = NULL;
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
                      "",
                      "in_",
                      "blk_") 
{
  _expr_file_path = expr_file_path;
  path = get_cache_loc();
  runtime_accessed_set.clear ();

  bool invalidate_cache = false;
  if (config_exists("synth.expropt.cache.invalidate")) {
    invalidate_cache = (config_get_int("synth.expropt.cache.invalidate") != 0);
  }

  config_set_default_string("synth.expropt.cache.cell_lib_namespace", "syn");
    
  // things to find and replace when storing in cache
  // just store the verilog file

  if (invalidate_cache) {
    Assert(!(path.empty()), "what");
    std::string del_files_cmd = std::string("rm ") + std::string(path);
    system(del_files_cmd.c_str());
  }

  fs::path cache_path = path;
  if (!fs::exists(cache_path)) {
    sqlite3 *db;
    int rc;
    rc = sqlite3_open(path.c_str(), &db);
    if (rc) {
      std::cerr << "Could not create cache: " << cache_path << std::endl;
      sqlite3_close (db);
      exit(1);
    }

    // now we create the tables
    const char *sql[] = {
      R"(CREATE TABLE IF NOT EXISTS
	entries (
            expr TEXT UNIQUE NOT NULL,
            id INTEGER PRIMARY KEY AUTOINCREMENT ); )",
      R"(CREATE TABLE IF NOT EXISTS
        metrics (
            id INTEGER PRIMARY KEY,
            delay_min REAL,
            delay_typ REAL,
            delay_max REAL,
            static_power_min REAL,
            static_power_typ REAL,
            static_power_max REAL,
            dynamic_energy_min REAL,
            dynamic_energy_typ REAL,
            dynamic_energy_max REAL,
            total_power_min REAL,
            total_power_typ REAL,
            total_power_max REAL,
            area REAL,
            mapper_runtime REAL,
            io_runtime REAL ); )",
      R"(CREATE TABLE IF NOT EXISTS
        data (
           id INTEGER PRIMARY KEY,
           pre_v BLOB,
           mapped_v BLOB
        ); )",
      NULL };

    sqlite3_stmt *stmt;

    for (int i=0; sql[i]; i++) {
      rc = sqlite3_prepare_v2 (db, sql[i], -1, &stmt, NULL);
      if (rc != SQLITE_OK) {
	std::cerr << "Unexpected error in cache creation: " <<
	  sqlite3_errmsg (db) << std::endl;
	sqlite3_close (db);
	exit (1);
      }
      rc = sqlite3_step (stmt);
      if (rc != SQLITE_DONE) {
	std::cerr << "Unexpected error in cache creation step: " <<
	  sqlite3_errmsg (db) << std::endl;
	sqlite3_close (db);
	exit (1);
      }
      sqlite3_finalize (stmt);
    }
    sqlite3_close (db);
  }
}

std::string ExprCache::_gen_unique_id (Expr *e, iHashtable *expr_map, 
                        iHashtable *width_map, int outwidth)
{
    list_t *vars = list_new();
    act_expr_collect_ids (vars, e);
    std::string uniq_id = act_expr_to_string(vars, e);
    std::string io_signature = "";

    std::unordered_map<ActId *, Expr *> id_to_expr = {};
    ihash_iter_t iter;
    ihash_bucket_t *ib;
    ihash_iter_init (expr_map, &iter);
    while ((ib = ihash_iter_next (expr_map, &iter))) 
    {
        Expr *e1 = (Expr *)ib->key;
        id_to_expr.insert({(ActId *)(e1->u.e.l), e1});
    }

    for (listitem_t *li = list_first(vars); li; li = li->next) 
    {
        auto id = (ActId *)(list_value(li));
        auto b = ihash_lookup(width_map, (long)(id_to_expr.at(id)));
        Assert (b, "var. width not found");
        int width = b->i;
        // gotta append bitwidth   
        io_signature.append("_");
        io_signature.append(std::to_string(width));
    }
    io_signature.append("_");
    io_signature.append(std::to_string(outwidth));

    uniq_id.append(io_signature);
    return uniq_id;
}

/**
 * XXX: change this to incremental reading of a file and writing a blob.
 **/
static
std::string gzString (const std::string &data)
{
  z_stream zs;
  memset (&zs, 0, sizeof (zs));
  if (deflateInit (&zs, Z_BEST_COMPRESSION) != Z_OK) {
    std::cerr << "Compression error!" << std::endl;
    exit (1);
  }
  zs.next_in = (Bytef*) data.data();
  zs.avail_in = data.size ();
  
  int ret;
  char outbuffer[1024];
  std::string outstring;
  do {
    zs.next_out = (Bytef*) (outbuffer);
    zs.avail_out = sizeof (outbuffer);
    ret = deflate (&zs, Z_FINISH);
    if (outstring.size() < zs.total_out) {
      outstring.append (outbuffer, zs.total_out - outstring.size());
    }
  } while (ret == Z_OK);
  deflateEnd (&zs);

  if (ret != Z_STREAM_END) {
    std::cerr << "Compression error!" << std::endl;
    exit (1);
  }    
  return outstring;
}


/**
 * XXX: change this to incremental reading of a blob and writing a
 * file
 **/
static
std::string guzString (const char *dat, int len)
{
  z_stream zs;
  memset (&zs, 0, sizeof (zs));
  if (inflateInit (&zs) != Z_OK) {
    std::cerr << "Decompression error-start!" << std::endl;
    exit (1);
  }
  zs.next_in = (Bytef*) dat;
  zs.avail_in = len;
  
  int ret;
  int have;
  char outbuffer[1024];
  std::string outstring;
  do {
    zs.next_out = (Bytef*) (outbuffer);
    zs.avail_out = sizeof (outbuffer);
    
    ret = inflate (&zs, Z_NO_FLUSH);
    have = sizeof (outbuffer) - zs.avail_out;
    outstring.append (outbuffer, have);
  } while (ret == Z_OK);
  inflateEnd (&zs);

  if (ret != Z_STREAM_END) {
    std::cerr << "Decompression error-end!" << std::endl;
    if (ret == Z_ERRNO) {
      std::cerr << "stream error?" << std::endl;
    }
    else if (ret == Z_STREAM_ERROR) {
      std::cerr << "invalid compression level" << std::endl;
    }
    else if (ret == Z_DATA_ERROR) {
      std::cerr << "invalid/incomplete data" << std::endl;
    }
    else if (ret == Z_MEM_ERROR) {
      std::cerr << "out of memory" << std::endl;
    }
    else if (ret == Z_VERSION_ERROR) {
      std::cerr << "version mismatch in zlib" << std::endl;
    }
    else if (ret == Z_BUF_ERROR) {
      std::cerr << "zbuf error" << std::endl;
    }
    else {
      std::cerr << "Z is " << ret << " / " << Z_OK << std::endl;
    }
    exit (1);
  }
  return outstring;
}


static int db_get_idx (sqlite3 *db, const std::string &str)
{
  const char *sql = "SELECT id from entries where expr = ?";
  sqlite3_stmt *stmt;
  int rc;

  rc = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    std::cerr << "Unexpected error (prepare) in searching cache: " <<
      sqlite3_errmsg (db) << std::endl;
    sqlite3_close (db);
    exit (1);
  }
  rc = sqlite3_bind_text (stmt, 1, str.c_str(), -1, NULL);
  if (rc != SQLITE_OK) {
    std::cerr << "Unexpected error (bind_text) in searching cache: " <<
      sqlite3_errmsg (db) << std::endl;
    sqlite3_close (db);
    exit (1);
  }
  rc = sqlite3_step (stmt);
  int idx;
  if (rc == SQLITE_ROW) {
    // found the row!
    idx = sqlite3_column_int (stmt, 0);
  }
  else {
    idx = -1;
  }
  sqlite3_finalize (stmt);
  return idx;
}

static int db_gen_idx (sqlite3 *db, const std::string &str)
{
  const char *sql = "insert into entries (expr) values (?)";
  sqlite3_stmt *stmt;
  int rc;
  
  sqlite3_exec (db, "BEGIN;", NULL, NULL, NULL);
  rc = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    std::cerr << "Unexpected error (prepare2) in update cache: " <<
      sqlite3_errmsg (db) << std::endl;
    sqlite3_close (db);
    exit (1);
  }
  rc = sqlite3_bind_text (stmt, 1, str.c_str(), -1, NULL);
  if (rc != SQLITE_OK) {
    std::cerr << "Unexpected error (bind_text2) in update cache: " <<
      sqlite3_errmsg (db) << std::endl;
    sqlite3_close (db);
    exit (1);
  }
  int retry = 15;
  do {
    rc = sqlite3_step (stmt);
    if (rc == SQLITE_CONSTRAINT) {
      sqlite3_finalize (stmt);
      sqlite3_exec (db, "ROLLBACK;", NULL,  NULL, NULL);
      return -1;
    }
    if (rc == SQLITE_BUSY) {
      retry--;
    }
  } while (retry > 0 && rc != SQLITE_DONE);

  if (rc != SQLITE_DONE) {
    if (rc == SQLITE_BUSY) {
      std::cerr << "Database cache access is locked for too long; giving up."
		<< std::endl;
    }
    else {
      std::cerr << "Unexpected error in updating cache: " <<
	sqlite3_errmsg (db) << std::endl;
    }
    sqlite3_exec (db, "ROLLBACK;", NULL,  NULL, NULL);
    sqlite3_close (db);
    exit (1);
  }
  sqlite3_finalize (stmt);
  return db_get_idx (db, str);
}

ExprBlockInfo *ExprCache::synth_expr (int targetwidth,
                                      Expr *expr,
                                      list_t *in_expr_list,
                                      iHashtable *in_expr_map,
                                      iHashtable *in_width_map)
{
  std::string uniq_id = _gen_unique_id(expr, in_expr_map, in_width_map, targetwidth);
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int rc;
  const char *sql;

  std::string loc = get_cache_loc();

  rc = sqlite3_open (loc.c_str(), &db);
  if (rc) {
    std::cerr << "Could not open cache database: " << loc << std::endl;
    sqlite3_close (db);
    exit (1);
  }
  rc = sqlite3_busy_timeout (db, 5000); // 5 second timeout on locks

  int idx = db_get_idx (db, uniq_id);
  bool rollback = true;

  auto errmsg = [&](const char *msg) {
    std::cerr << "Unexpected error (" << msg << ") in update cache: " <<
      sqlite3_errmsg (db) << std::endl;
    if (rollback) {
      sqlite3_exec (db, "ROLLBACK;", NULL, NULL, NULL);
    }
    sqlite3_close (db);
    exit (1);
  };

  auto errcheck = [&](int res, const char *msg) {
    if (rc != SQLITE_OK) {
      errmsg (msg);
    }
  };
  
  ExprBlockInfo *ebi = NULL;
  bool from_cache = (idx == -1 ? false : true);

  if (!from_cache) {
    /* did not find this in the cache, so create a new cache entry */
    
    idx = db_gen_idx (db, uniq_id);
    
    /*
      Entry creation might result in an error because someone else
      concurrently created the same entry. In this case we get a -1
      return value.
    */ 
    if (idx == -1) {
      /* Get the newly created entry and switch to using the cache */
      idx = db_get_idx (db, uniq_id);
      if (idx == -1) {
	/* This is a problem... so error out! */
	std::cerr << "Unexpected database error! Inserted expr not found!" << std::endl;
	sqlite3_close (db);
	exit (1);
      }
      else {
	/* We found it, so switch to the cache */
	from_cache = true;
      }
    }
  }
  
  if (from_cache) {
    rollback = false;
    // found the row!

    // we found it in the database
    // now access the other two tables to construct
    // 1. the metrics
    // 2. the files
    // create a temp mapped file name
    std::string fname = gen_mapped_filename ();

    // grab the expression block info and save into ebi
    sql = "select * from metrics where id = ?";
    rc = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
    errcheck (rc, "prepare-read");
    rc = sqlite3_bind_int (stmt, 1, idx);
    errcheck (rc, "bind-ir1");
    rc = sqlite3_step (stmt);
    if (rc != SQLITE_ROW) {
      errmsg ("metrics fetch error");
    }
    // get columns using sqlite3_column_..
    
    double vals[15];
    for (int i=0; i < 15; i++) {
      vals[i] = sqlite3_column_double (stmt, i+1);
    }
    sqlite3_finalize (stmt);

    metric_triplet delay;
    delay.set_metrics( vals[0], vals[1], vals[2] );
    metric_triplet static_power;
    static_power.set_metrics (vals[3], vals[4], vals[5]);
    
    metric_triplet dynamic_energy;
    dynamic_energy.set_metrics(vals[6], vals[7], vals[8]);
    
    metric_triplet total_power;
    total_power.set_metrics (vals[9], vals[10], vals[11]);

    sql = "select mapped_v from data where id = ?";
    rc = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
    errcheck (rc, "prepare-read2");
    rc = sqlite3_bind_int (stmt, 1, idx);
    errcheck (rc, "bind-ir2");
    rc = sqlite3_step (stmt);
    if (rc != SQLITE_ROW) {
      errmsg ("metrics fetch error");
    }
    // get columns using sqlite3_column_..
    
    int len = sqlite3_column_bytes (stmt, 0);
    const char *dat = (const char *) sqlite3_column_blob (stmt, 0);
    std::string res = guzString (dat, len);

    std::ofstream mapped(fname);
    if (mapped.is_open()) {
      mapped << res;
      mapped.close();
    }
    else {
      std::cerr << "Could not create file: " << fname << std::endl;
      exit (1);
    }
    sqlite3_finalize (stmt);

    ebi = new ExprBlockInfo(delay, static_power, dynamic_energy,
			    total_power, vals[12], vals[13], vals[14],
			    fname, "", uniq_id);
  }
  else {
    /* we need to run synthesis and prepare everything */
    ebi = run_external_opt(uniq_id, targetwidth, expr, 
			   in_expr_list, in_expr_map, in_width_map, false);
    
    ebi->setID(uniq_id);
    auto verilogfile = ebi->getMappedFile();
    auto presynfile = ebi->getUnmappedFile();

    std::string vblob;
    {
      std::ifstream src(presynfile);
      std::ostringstream buf;
      buf << src.rdbuf();
      vblob = gzString (buf.str());
    }
    std::string vmapblob;
    {
      std::ifstream src(verilogfile);
      std::ostringstream buf;
      buf << src.rdbuf();
      vmapblob = gzString (buf.str());
    }
    // at this point, we need to update the database blobs and metrics

    sql = "INSERT INTO metrics (id, delay_min, delay_typ, delay_max, static_power_min, static_power_typ, static_power_max, dynamic_energy_min, dynamic_energy_typ, dynamic_energy_max, total_power_min, total_power_typ, total_power_max, area, mapper_runtime, io_runtime) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    double vals[15];
    vals[0] = ebi->getDelay().min_val;
    vals[1] = ebi->getDelay().typ_val;
    vals[2] = ebi->getDelay().max_val;
    
    vals[3] = ebi->getStaticPower().min_val;
    vals[4] = ebi->getStaticPower().typ_val;
    vals[5] = ebi->getStaticPower().max_val;
    
    vals[6] = ebi->getDynamicPower().min_val;
    vals[7] = ebi->getDynamicPower().typ_val;
    vals[8] = ebi->getDynamicPower().max_val;
    
    vals[9] = ebi->getPower().min_val;
    vals[10] = ebi->getPower().typ_val;
    vals[11] = ebi->getPower().max_val;
    
    vals[12] = ebi->getArea();
    vals[13] = ebi->getRuntime();
    vals[14] = ebi->getIORuntime();

    rc = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
    errcheck (rc, "prepare3");
    rc = sqlite3_bind_int (stmt, 1, idx);
    errcheck (rc, "bind_int3");

    for (int i=0; i < 15; i++) {
      rc = sqlite3_bind_double (stmt, 2+i, vals[i]);
      errcheck (rc, "bind_double");
    }
      
    rc = sqlite3_step (stmt);
    if (rc != SQLITE_DONE) {
      errmsg ("step-2");
    }
    sqlite3_finalize (stmt);
    
    sql = "INSERT INTO data (id, pre_v, mapped_v) VALUES (?, ?, ?)";
    rc = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
    errcheck (rc, "prepare4");

    rc = sqlite3_bind_int (stmt, 1, idx);
    errcheck (rc, "bind_int4");

    rc = sqlite3_bind_blob (stmt, 2, vblob.data(), vblob.size(), NULL);
    errcheck (rc, "bind_blob1");
    
    rc = sqlite3_bind_blob (stmt, 3, vmapblob.data(), vmapblob.size(), NULL);
    errcheck (rc, "bind_blob2");

    rc = sqlite3_step (stmt);
    if (rc != SQLITE_DONE) {
      errmsg ("step-3");
    }
    sqlite3_finalize (stmt);
    sqlite3_exec (db, "COMMIT;", NULL, NULL, NULL);
  }

  if (!runtime_accessed_set.contains (uniq_id)) {
    std::chrono::microseconds dummy;
    set_expr_outfile (_expr_file_path);
    auto *tmp = backend(ebi->getMappedFile(), "", dummy, dummy);
    delete tmp;
    runtime_accessed_set.insert(uniq_id);
  }
  
  if (from_cache) {
    if (_cleanup) {
      unlink (ebi->getMappedFile().c_str());
    }
  }
  else {
    cleanup_tmp_files ();
  }
  
  sqlite3_close (db);
  
  return ebi;
}

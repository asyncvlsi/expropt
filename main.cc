/*************************************************************************
 *
 *  Copyright (c) 2026 Rajit Manohar
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
#include <sqlite3.h>
#include <act/act.h>
#include "expr_cache.h"
#include <iostream>
#include <ctype.h>
#include <string>
#include <zlib.h>


void usage (const char *name)
{
  fprintf (stderr, "Usage: %s (abc|yosys|...) <command>\n", name);
  fprintf (stderr, R"(The command can be:
   info                : displays information about the expression cache
   getid <str>         : displays expression id for expression string <str>
   metrics <num>       : displays metrics for expression id <num>
   get_v <num> <file>  : write verilog for expr id <num> to <file>
   get_mv <num> <file> : write mapped verilog for expr id <num> to <file>
)");
  exit (1);
}


void chk_error2 (sqlite3 *db, int rc, int rc_expect, const char *msg)
{
  if (rc != rc_expect) {
    std::cerr << "Unexpected error (" << msg << "): " <<
      sqlite3_errmsg (db) << std::endl;
    sqlite3_close (db);
    exit (1);
  }
}

void chk_error (sqlite3 *db, int rc, const char *msg)
{
  chk_error2 (db, rc, SQLITE_OK, msg);
}


void run_info (sqlite3 *db)
{
  sqlite3_stmt *stmt;
  const char *sql;
  int rc;

  sql = "select * from entries";
  rc = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
  chk_error (db, rc, "info: prepare");
  rc = sqlite3_step (stmt);
  while (rc == SQLITE_ROW) {
    const unsigned char *txt = sqlite3_column_text (stmt, 0);
    int idx = sqlite3_column_int (stmt, 1);
    printf ("%12d %s\n", idx, txt);
    rc = sqlite3_step (stmt);
  }
  chk_error2 (db, rc, SQLITE_DONE, "info: read table");
  sqlite3_finalize (stmt);
}

void run_metrics (sqlite3 *db, int num)
{
  if (num < 1) {
    printf ("Error: number is less than 1!\n");
    return;
  }
  sqlite3_stmt *stmt;
  const char *sql;
  int rc;

  sql = "select * from metrics where id = ?";

  rc = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
  chk_error (db, rc, "metrics: prepare");
  
  rc = sqlite3_bind_int (stmt, 1, num);
  chk_error (db, rc, "metrics: bind ind");

  rc = sqlite3_step (stmt);
  int idx;
  if (rc == SQLITE_ROW) {
    double v[15];
    for (int i=0; i < 15; i++) {
      v[i] = sqlite3_column_double (stmt, i+1);
    }
    printf ("Metrics:\n");
    printf ("  Delay (s): %.4g (min %.4g, max %.4g)\n",
	    v[1], v[0], v[2]);
    printf ("  Static power (W): %.4g (min %.4g, max %.4g)\n",
	    v[4], v[3], v[5]);
    printf ("  Dynamic energy (J): %.4g (min %.4g, max %.4g)\n",
	    v[7], v[6], v[8]);
    printf ("  Area (um^2): %.4g\n", v[12]);
  }
  else {
    printf ("Error: could not find metrics for index %d\n", num);
  }
  sqlite3_finalize (stmt);
}

void run_getid (sqlite3 *db, char *name)
{
  sqlite3_stmt *stmt;
  const char *sql;
  int rc;

  sql = "select id from entries where expr = ?";

  rc = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
  chk_error (db, rc, "getid: prepare");
  
  rc = sqlite3_bind_text (stmt, 1, name, -1, NULL);
  chk_error (db, rc, "getid: bind text");

  rc = sqlite3_step (stmt);
  int idx;
  if (rc == SQLITE_ROW) {
    idx = sqlite3_column_int (stmt, 0);
  }
  else {
    idx = -1;
  }
  sqlite3_finalize (stmt);
  if (idx == -1) {
    printf ("Error: could not find expression `%s'\n", name);
    return;
  }
  printf ("Expression `%s' is id %d\n", name, idx);
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


void run_getv (sqlite3 *db, int num, char *file, bool mapped)
{
  sqlite3_stmt *stmt;
  const char *sql;
  int rc;

  if (num < 1) {
    printf ("Error: invalid integer `%d'\n", num);
    return;
  }

  if (mapped) {
    sql = "select mapped_v from data where id = ?";
  }
  else {
    sql = "select pre_v from data where id = ?";
  }
  rc = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
  chk_error (db, rc, "getv prepare");
  rc = sqlite3_bind_int (stmt, 1, num);
  chk_error (db, rc, "getv bind int");
  rc = sqlite3_step (stmt);
  if (rc != SQLITE_ROW) {
    printf ("Error: could not find expression `%d'\n", num);
    sqlite3_finalize (stmt);
    return;
  }
  int len = sqlite3_column_bytes (stmt, 0);
  const char *dat = (const char *) sqlite3_column_blob (stmt, 0);
  std::string res = guzString (dat, len);
  std::ofstream output(file);
  if (output.is_open()) {
    output << res;
    output.close ();
  }
  else {
    printf ("Error: could not create file `%s'\n", file);
  }
  sqlite3_finalize (stmt);
}
  

int main (int argc, char **argv)
{
  char *s;
  Act::Init (&argc, &argv);
  if (argc < 3) usage (argv[0]);
  ExprCache *ec = new ExprCache (argv[1], expr_mapping_target::qdi, false);

  std::string loc = ec->get_cache_loc();
  std::cout << "Cache location: " << loc << std::endl;

  int rc;
  sqlite3 *db;
  rc = sqlite3_open (loc.c_str(), &db);
  chk_error (db, rc, "database open");

  if (strcmp (argv[2], "info") == 0) {
    if (argc != 3) {
      sqlite3_close (db);
      usage (argv[0]);
    }
    run_info (db);
  }
  else if (strcmp (argv[2], "getid") == 0) {
    if (argc != 4) {
      sqlite3_close (db);
      usage (argv[0]);
    }
    run_getid (db, argv[3]);
  }
  else if (strcmp (argv[2], "metrics") == 0) {
    if (argc != 4) {
      sqlite3_close (db);
      usage (argv[0]);
    }
    run_metrics (db, atoi(argv[3]));
  }
  else if (strcmp (argv[2], "get_v") == 0) {
    if (argc != 5) {
      sqlite3_close (db);
      usage (argv[0]);
    }
    run_getv (db, atoi(argv[3]), argv[4], false);
  }
  else if (strcmp (argv[2], "get_mv") == 0) {
    if (argc != 5) {
      sqlite3_close (db);
      usage (argv[0]);
    }
    run_getv (db, atoi(argv[3]), argv[4], true);
  }
  else {
    sqlite3_close (db);
    usage (argv[0]);
  }
  sqlite3_close (db);
  return 0;
}  

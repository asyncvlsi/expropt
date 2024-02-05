 /*************************************************************************
 *
 *  This file is part of act expropt
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
 **************************************************************************/

#include "expropt.h"

#include "config_pkg.h"

#ifdef FOUND_exproptcommercial
#include <act/exproptcommercial.h>
#endif

#ifndef FOUND_abc
#error "Please use cmake to install the abc package from https://github.com/asyncvlsi/abc"
#endif

#include <act/types.h>
#include <common/int.h>
#include <string.h>
#include "abc_api.h"

/**
 * Destroy the External Expr Opt:: External Expr Opt object
 * 
 * no memory allocation on global vars, nothing to be done
 */
ExternalExprOpt::~ExternalExprOpt()
{
  if (_abc_api) {
    AbcApi *x = (AbcApi *) _abc_api;
    delete x;
  }
}

/*
 * the a wrapper for chp2prs to just run the optimisation with a single expression,
 * 
 */
ExprBlockInfo* ExternalExprOpt::run_external_opt (int expr_set_number, int targetwidth, Expr *expr, list_t *in_expr_list, iHashtable *in_expr_map, iHashtable *in_width_map)
{
  //build the data structures need
  ExprBlockInfo* info;
  iHashtable *outexprmap = ihash_new(0);
  iHashtable *inexprmap = ihash_new(0);
  iHashtable *outwidthmap = ihash_new(0);
  list_t *outlist = list_new();

  // convert input list, reverse searching nessesary, should always use last on multimatching
  listitem_t *li;
  Expr* e = NULL;
  for (li = list_first (in_expr_list); li; li = list_next (li))
  { 
    e = (Expr *) list_value(li);
    // change from int to C string
    ihash_bucket_t *b_map,*b_new;
    b_map = ihash_lookup(in_expr_map, (long) e);
    char *charbuf = (char *) malloc( sizeof(char) * ( 1024 + 1 ) );
    sprintf(charbuf,"%s%u",expr_prefix.data(),b_map->i);
    b_new = ihash_add(inexprmap, (long) e);
    b_new->v = charbuf;
  }

  ihash_bucket_t *b_map;
  char *charbuf = (char *) malloc( sizeof(char) * ( 1024 + 1 ) );
  sprintf(charbuf,"out");
  b_map = ihash_add(outexprmap,(long) expr);
  b_map->v = charbuf;
  ihash_bucket_t *b_width;
  b_width = ihash_add(outwidthmap,(long) expr);
  list_append(outlist, expr);
  b_width->i = targetwidth;

  // generate module name 
  char expr_set_name[1024];
  sprintf(expr_set_name, "%s%u",module_prefix.data(), expr_set_number);
  // and send off
  info = run_external_opt(expr_set_name,in_expr_list,inexprmap,in_width_map,outlist,outexprmap,outwidthmap);
  // after completerion clean up memory, the generated char names will leak they are not cleaned up atm.
  list_free(outlist);
  ihash_free(outexprmap);
  ihash_free(outwidthmap);
  ihash_free(inexprmap);
  return info;
}

/*
 * the wrapper for chp2prs to run sets of expressions like guards, uses chp2prs data structures and converts them to ExprOpt standart
 */
ExprBlockInfo* ExternalExprOpt::run_external_opt (int expr_set_number, list_t *expr_list, list_t *in_list, list_t *out_list, iHashtable *exprmap_int)
{
  //build the data structures need
  ExprBlockInfo* info;
  iHashtable *inexprmap = ihash_new(0);
  iHashtable *inwidthmap = ihash_new(0);
  iHashtable *outexprmap = ihash_new(0);
  iHashtable *outwidthmap = ihash_new(0);
  list_t *outlist = list_new();
  list_t *inlist = list_new();

  // convert input list, reverse searching nessesary, should always use last on multimatching
  listitem_t *li;
  Expr* e = NULL;
  for (li = list_first (in_list); li; li = list_next (li))
  { int i;
    ihash_bucket_t *b_search;
    for (i = 0; i < exprmap_int->size; i++)
    {
      for (b_search = exprmap_int->head[i]; b_search; b_search = b_search->next) {
        if (b_search->i == list_ivalue (li)){
          i = exprmap_int->size;
          e = (Expr *) b_search->key;
          break;
        }
      }	
    }
    // construct new data object, put width in in_width_map, generate expression name out of ID for in_expr_map
    if (!e) fatal_error("expression lookup failed, expression not found");
    ihash_bucket_t *b_map;
    char *charbuf = (char *) malloc( sizeof(char) * ( 1024 + 1 ) );
    sprintf(charbuf,"%s%u",expr_prefix.data(),list_ivalue (li));
    b_map = ihash_add(inexprmap,(long) e);
    b_map->v = charbuf;
    ihash_bucket_t *b_width;
    b_width = ihash_add(inwidthmap,(long) e);
    list_append(inlist, e);
    li = list_next (li);
    b_width->i = list_ivalue (li);
  }
  // do the exact same for the outlist!
  for (li = list_first (out_list); li; li = list_next (li))
  { int i;
    ihash_bucket_t *b_search;
    for (i = 0; i < exprmap_int->size; i++)
    {
      for (b_search = exprmap_int->head[i]; b_search; b_search = b_search->next) {
        if (b_search->i == list_ivalue (li)){
          i = exprmap_int->size;
          e = (Expr *) b_search->key;
          break;
        }
      }	
    }
    if (!e) fatal_error("expression lookup failed, expression not found");
    ihash_bucket_t *b_map;
    char *charbuf = (char *) malloc( sizeof(char) * ( 1024 + 1 ) );
    sprintf(charbuf,"%s%u",expr_prefix.data(),list_ivalue (li));
    b_map = ihash_add(outexprmap,(long) e);
    b_map->v = charbuf;
    ihash_bucket_t *b_width;
    b_width = ihash_add(outwidthmap,(long) e);
    list_append(outlist, e);
    li = list_next (li);
    b_width->i = list_ivalue (li);
  }
  // generate module name 
  char expr_set_name[1024];
  sprintf(expr_set_name, "%s%u",module_prefix.data(), expr_set_number);
  // and send off
  info = run_external_opt(expr_set_name,inlist,inexprmap,inwidthmap,outlist,outexprmap,outwidthmap);
  // after completerion clean up memory, the generated char names will leak they are not cleaned up atm.
  list_free(outlist);
  list_free(inlist);
  ihash_free(inexprmap);
  ihash_free(inwidthmap);
  ihash_free(outexprmap);
  ihash_free(outwidthmap);
  return info;
}

/**
 * first constuct the filenames for the temporary files and than generate the verilog, exc the external tool, read out the results and convert them back to act.
 * 
 * the printing of the verilog is seperate
 */
ExprBlockInfo* ExternalExprOpt::run_external_opt (const char* expr_set_name, list_t *in_expr_list, iHashtable *in_expr_map, iHashtable *in_width_map, list_t *out_expr_list, iHashtable *out_expr_map, iHashtable *out_width_map, list_t *hidden_expr_list)
{

  //build the data structures need
  ExprBlockInfo* info;
  list_t *out_name_list = list_new();
  list_t *hidden_name_list = NULL;
  listitem_t *li;
  if (hidden_expr_list) 
  {
    hidden_name_list = list_new();
    for (li = list_first (hidden_expr_list); li; li = list_next (li))
    { 
      ihash_bucket_t *b;
      b = ihash_lookup (out_expr_map, (long) list_value(li));
      Assert (b, "variable not found in variable map");
      list_append(hidden_name_list, b->v);
    }
  }

  for (li = list_first (out_expr_list); li; li = list_next (li))
  { 
    ihash_bucket_t *b;
    b = ihash_lookup (out_expr_map, (long) list_value(li));
    Assert (b, "variable not found in variable map");
    list_append(out_name_list, b->v);
  }

  info = run_external_opt(expr_set_name,in_expr_list,in_expr_map,in_width_map,out_expr_list,out_name_list,out_width_map,hidden_expr_list,hidden_name_list);
  
  list_free(out_name_list);
  list_free(hidden_name_list);
  return info;

}

/*
 * This is a hack, fix this later...
 * Taken from OSU .lib file
 */
static struct cell_info {
  const char *name;
  double area;
  int count;
} _cell_info[] = {
	  { "AND2X1", 32, 0 },
	  { "AND2X2", 32, 0 },
	  { "AOI21X1", 32, 0 },
	  { "AND2X1", 32, 0 },
	  { "AOI22X2", 40, 0 },
	  { "BUFX2", 24, 0 },
	  { "BUFX4", 32, 0 },
	  { "CLKBUF1", 72, 0 },
	  { "CLKBUF2", 104, 0 },
	  { "CLKBUF3", 136, 0 },
	  { "FAX1", 120, 0 },
	  { "HAX1", 80, 0 },
	  { "INVX1", 16, 0 },
	  { "INVX2", 16, 0 },
	  { "INVX4", 24, 0 },
	  { "INVX8", 40, 0 },
	  { "LATCH", 16, 0 },
	  { "MUX2X1", 48, 0 },
	  { "NAND2X1", 24, 0 },
	  { "NAND3X1", 36, 0 },
	  { "NOR2X1", 24, 0 },
	  { "NOR3X1", 64, 0 },
	  { "OAI21X1", 24, 0 },
	  { "OAI22X1", 40, 0 },
	  { "OR2X1", 32, 0 },
	  { "OR2X2", 32, 0 },
	  { "TBUFX1", 40, 0 },
	  { "TBUFX2", 56, 0 },
	  { "XNOR2X1", 56, 0 },
	  { "XOR2X1", 56, 0 }
};

static double parse_abc_info (bool yosys_mode, const char *file, double *area)
{
  char buf[10240];
  FILE *fp;
  double ret;
  int i;


  snprintf (buf, 10240, "%s.log", file);
  fp = fopen (buf, "r");
  if (!fp) {
    return -1;
  }

  ret = -1;
  for (i=0; i < sizeof (_cell_info)/sizeof (_cell_info[0]); i++) {
    _cell_info[i].count = 0;
  }

  while (fgets (buf, 10240, fp)) {
    if (yosys_mode == false || (strncmp (buf, "ABC:", 4) == 0)) {
      char *tmp = strstr (buf, "Delay =");
      if (tmp) {
	if (sscanf (tmp, "Delay = %lf ps", &ret) == 1) {
	  ret = ret*1e-12;
	}
      }
    }
    else if (strncmp (buf, "ABC RESULTS:", 12) == 0) {
      char *tmp = buf + 12;
      while (*tmp && isspace (*tmp)) {
	tmp++;
      }
      if (*tmp) {
	char *cell_name = tmp;
	while (*tmp && !isspace (*tmp)) {
	  tmp++;
	}
	if (*tmp) {
	  *tmp = '\0';
	  tmp++;
	}
	int count = -1;
	if (strncmp (tmp, "cells:", 6) == 0) {
	  tmp += 6;
	  if (sscanf (tmp, "%d", &count) != 1) {
	    count = -1;
	  }
	}
	if (*tmp && count > 0) {
	  int i;
	  for (i=0; i < sizeof (_cell_info)/sizeof (_cell_info[0]); i++) {
	    if (strcmp (cell_name, _cell_info[i].name) == 0) {
	      _cell_info[i].count++;
	      break;
	    }
	  }
	  //printf ("got cell %s, count = %d\n", cell_name, count);
	  /* got cell count! */
	  /* XXX: the area is in the .lib file... */
	}
      }
    }
  }
  if (area) {
    int i;
    *area = 0;
    for (i=0; i < sizeof (_cell_info)/sizeof (_cell_info[0]); i++) {
      *area += _cell_info[i].area * _cell_info[i].count;
    }
  }
  fclose (fp);
  return ret;
}

/**
 * first constuct the filenames for the temporary files and than generate the verilog, exc the external tool, read out the results and convert them back to act.
 * 
 * the printing of the verilog is seperate
 */
ExprBlockInfo* ExternalExprOpt::run_external_opt (const char* expr_set_name, list_t *in_expr_list, iHashtable *in_expr_map, iHashtable *in_width_map, list_t *out_expr_list, list_t *out_expr_name_list, iHashtable *out_width_map, list_t *hidden_expr_list, list_t *hidden_expr_name_list)
{
  ExprBlockInfo* info = NULL;
  // consruct files names for the temp files
  std::string verilog_file = "./";
  verilog_file.append(VERILOG_FILE_PREFIX);
  verilog_file.append(expr_set_name);
  std::string mapped_file = verilog_file.data();
  verilog_file.append(".v");
  mapped_file.append(MAPPED_FILE_SUFFIX);
  mapped_file.append(".v");
  char cmd[4096] = "";
  FILE *verilog_stream;

  std::string sdc_file = "./";
  sdc_file.append(VERILOG_FILE_PREFIX);
  sdc_file.append(expr_set_name);
  sdc_file.append(".sdc");

  std::string lec_out = ".";
  lec_out.append ("/noneq.*.");
  lec_out.append (expr_set_name);
  lec_out.append (".*");

  // open temp verilog file to be syntesised

  verilog_stream = fopen(verilog_file.data(), "w");
  if (!verilog_stream) fatal_error("ExternalExprOpt::run_external_opt: verilog file %s is not writable",verilog_file.data());

  // generate verilog module

  if (mapper == abc) {
    /* abc is going to mess up the port names, so we need to fix that */
    char buf[1024];
    snprintf (buf, 1024, "%stmp", expr_set_name);
    print_expr_verilog(verilog_stream, buf, in_expr_list, in_expr_map, in_width_map, out_expr_list, out_expr_name_list, out_width_map, hidden_expr_list, hidden_expr_name_list);
  }
  else {
    print_expr_verilog(verilog_stream, expr_set_name, in_expr_list, in_expr_map, in_width_map, out_expr_list, out_expr_name_list, out_width_map, hidden_expr_list, hidden_expr_name_list);
  }

  // force write and close file

  fflush(verilog_stream);
  fclose(verilog_stream);

  // generate the exec command for the sysntesis tool and run the syntesis
  int exec_failure = 1;

#ifdef FOUND_exproptcommercial
  ExprOptCommercialHelper *helper = new ExprOptCommercialHelper();
#endif

  char *configreturn;
  AbcApi *a_api;
  
  switch (mapper)
  {
  case genus:
#ifdef FOUND_exproptcommercial
    if (expr_output_file.empty()) exec_failure = helper->run_genus(verilog_file,mapped_file,expr_set_name, true);
    else exec_failure = helper->run_genus(verilog_file,mapped_file,expr_set_name);
    break;
#else
    fatal_error("cadence genus support was not enabled on compile time");
    break;
#endif
    
  case synopsis:
    // would need a sample script to implement this
    fatal_error("synopsis compiler is not implemented yet");
    break;

  case abc:
    /* create a .sdc file to get delay values */
    verilog_stream = fopen (sdc_file.data(), "w");
    if (!verilog_stream) {
      fatal_error ("Could not open `%s' file!", sdc_file.data());
    }
    fprintf (verilog_stream, "set_load %g\n", config_get_real ("expropt.default_load"));
    fclose (verilog_stream);
    configreturn = config_get_string("expropt.liberty_tt_typtemp");

    if (strcmp (configreturn, "none") == 0) {
      fatal_error("please define \"liberty_tt_typtemp\" in expropt configuration file");
    }

    // expr_set_name is the name of the top-level module
    {
      if (config_get_int("expropt.verbose") == 2) printf("running: %s \n",cmd);
      else if (config_get_int("expropt.verbose") == 1) { printf("."); fflush(stdout); }
      if (_abc_api) {
	a_api = (AbcApi *) _abc_api;
      }
      else {
	a_api = new AbcApi ();
	_abc_api = a_api;
      }
      if (!a_api->startSession (expr_set_name)) {
	fatal_error ("Unable to start ABC session!");
      }

      if (!a_api->stdSynthesis ()) {
	fatal_error ("Unable to run logic synthesis using ABC api");
      }

      if (config_exists ("expropt.abc_use_constraints")) {
	if (config_get_int ("expropt.abc_use_constraints") == 1) {
	  if (!a_api->runTiming()) {
	    fatal_error ("Unable to run timing");
	  }
	}
      }
      if (!a_api->endSession ()) {
	fatal_error ("Unable to end session with ABC");
      }
    }
    break;
    
  case yosys:
    {
      /* create a .sdc file to get delay values */
      verilog_stream = fopen (sdc_file.data(), "w");
      if (!verilog_stream) {
	fatal_error ("Could not open `%s' file!", sdc_file.data());
      }
      fprintf (verilog_stream, "set_load %g\n", config_get_real ("expropt.default_load"));
      fclose (verilog_stream);
    }
  default:
    // yosys gets its script passed via stdin (very short)
    configreturn = config_get_string("expropt.liberty_tt_typtemp");
    if (strcmp(configreturn,"none") != 0)
    {
      int constr = 0;
      if (config_exists ("expropt.abc_use_constraints")) {
	if (config_get_int ("expropt.abc_use_constraints") == 1) {
	  constr = 1;
	}
      }
      if (use_tie_cells) {
	if (constr) {
	  sprintf(cmd,"echo \"read_verilog %s; synth -noabc -top %s; abc -constr %s -liberty %s; hilomap -hicell TIEHIX1 Y -locell TIELOX1 Y -singleton; write_verilog -nohex -nodec %s;\" | yosys > %s.log",verilog_file.data(), expr_set_name, sdc_file.data(), configreturn, mapped_file.data(), mapped_file.data());
	}
	else {
	  sprintf(cmd,"echo \"read_verilog %s; synth -noabc -top %s; abc -liberty %s; hilomap -hicell TIEHIX1 Y -locell TIELOX1 Y -singleton; write_verilog -nohex -nodec %s;\" | yosys > %s.log",verilog_file.data(), expr_set_name, configreturn, mapped_file.data(), mapped_file.data());
	}
      }
      else {
	if (constr) {
	  sprintf(cmd,"echo \"read_verilog %s; synth -noabc -top %s; abc -constr %s -liberty %s; write_verilog  -nohex -nodec %s;\" | yosys > %s.log", verilog_file.data(), expr_set_name, sdc_file.data(), configreturn, mapped_file.data(), mapped_file.data());
	}
	else {
	  sprintf(cmd,"echo \"read_verilog %s; synth -noabc -top %s; abc -liberty %s; write_verilog  -nohex -nodec %s;\" | yosys > %s.log", verilog_file.data(), expr_set_name, configreturn, mapped_file.data(), mapped_file.data());
	}
      }
    }
    else fatal_error("please define \"liberty_tt_typtemp\" in expropt configuration file");
    if (config_get_int("expropt.verbose") == 2) printf("running: %s \n",cmd);
    else if (config_get_int("expropt.verbose") == 1) { printf("."); fflush(stdout); }
    exec_failure = system(cmd);
    if (exec_failure != 0) fatal_error("yosys syntesis failed: \"%s\" failed.", cmd);
    // @TODO do metadata extraction via ABC
    break;
  }

  // read the resulting netlist and map it back to act, if the wire_type is not bool use the async mode the specify a wire type as a channel.
  // skip if run was just for extraction of properties => output filename empty
  if (!expr_output_file.empty())
  {
    if (expr_channel_type.compare("bool") != 0) sprintf(cmd,"v2act -a -C \"%s\" -l %s -n %s %s >> %s", expr_channel_type.data(), cell_act_file.data(), cell_namespace.data(), mapped_file.data(), expr_output_file.data());
    else sprintf(cmd,"v2act -l %s -n %s %s >> %s",cell_act_file.data(), cell_namespace.data(), mapped_file.data(), expr_output_file.data());
    if (config_get_int("expropt.verbose") == 2) printf("running: %s \n",cmd);
    else if (config_get_int("expropt.verbose") == 1) { printf("."); fflush(stdout); }
    exec_failure = system(cmd);
    if (exec_failure != 0) fatal_error("external program call \"%s\" failed.", cmd);
  }
  // parce block info - WORK IN PROGRESS
  switch (mapper)
  {
  case genus:
  {
#ifdef FOUND_exproptcommercial
    std::string genus_log = mapped_file.data();
    info = new ExprBlockInfo(             
                    helper->parse_genus_log(genus_log, metadata_delay_typ),
                    helper->parse_genus_log(genus_log, metadata_delay_min),
                    helper->parse_genus_log(genus_log, metadata_delay_max),
                    helper->parse_genus_log(genus_log, metadata_power_typ),
                    helper->parse_genus_log(genus_log, metadata_power_max),
                    helper->parse_genus_log(genus_log, metadata_area),
                    helper->parse_genus_log(genus_log, metadata_power_typ_static),
                    helper->parse_genus_log(genus_log, metadata_power_typ_dynamic),
                    helper->parse_genus_log(genus_log, metadata_power_max_static),
                    helper->parse_genus_log(genus_log, metadata_power_max_dynamic)
                    );
#else
    fatal_error("cadence genus support was not enabled on compile time");
#endif
  }
    break;
  case synopsis:
    fatal_error("synopsis compiler are not implemented yet");
    break;
  case yosys:
  case abc:
  default:
    { double delay, area;
      area = 0.0;
      delay = parse_abc_info (mapper == yosys ? true : false,
			      mapped_file.data(), &area);
      info =  new ExprBlockInfo(delay,
			      0, 0, 0, 0, 0, 0, 0, 0, area);
    }
    break;
  }

  // clean up temporary files
  if (cleanup) {
    switch (mapper)
    {
    case genus:
      sprintf(cmd,"rm %s && rm %s && rm %s.* && rm %s.* && rm -r fv* && rm -r rtl_fv* && rm %s && rm genus.*", mapped_file.data(), verilog_file.data(), mapped_file.data(), verilog_file.data(), lec_out.data());
      break;
    case synopsis:
    case abc:
    case yosys:
    default:
      sprintf(cmd,"rm %s && rm %s && rm %s && rm %s.* ", mapped_file.data(), verilog_file.data(),
	      sdc_file.data(), mapped_file.data());
      break;
    }
    if (config_get_int("expropt.verbose") == 2) printf("running: %s \n",cmd);
    else if (config_get_int("expropt.verbose") == 1) { printf("."); fflush(stdout); }
    exec_failure = system(cmd);
    if (exec_failure != 0) warning("external program call \"%s\" failed.", cmd);
  }
  // add exports for namespace support
  // @TODO sed calls are dangorus they change behavior depending on the version installed
#if 0
  if (!expr_output_file.empty())
  {
    sprintf(cmd,"sed -e 's/defproc/export defproc/' -e 's/export export/export/' %s > %sx && mv -f %sx %s", expr_output_file.data(),expr_output_file.data(),
	    expr_output_file.data(), expr_output_file.data());
    if (config_get_int("expropt.verbose") == 2) printf("running: %s \n",cmd);
    else if (config_get_int("expropt.verbose") == 1) { printf("."); fflush(stdout); }
    exec_failure = system(cmd);
    if (exec_failure != 0) fatal_error("external program call \"%s\" failed.", cmd);

    // remove directions on in and outputs to avoid errors on passthrough connections
    sprintf(cmd,"sed -e 's/[\\?!]//g' %s > %sx && mv -f %sx %s",
	    expr_output_file.data(), expr_output_file.data(),
	    expr_output_file.data(), expr_output_file.data());
    
    if (config_get_int("expropt.verbose") == 2) printf("running: %s \n",cmd);
    else if (config_get_int("expropt.verbose") == 1) { printf("."); fflush(stdout); }
    exec_failure = system(cmd);
    if (exec_failure != 0) fatal_error("external program call \"%s\" failed.", cmd);
  }
#endif
  return info;
}

/*
 * print the verilog module with header, in and outputs. call the expression print method for the assigns rhs.
 */
void ExternalExprOpt::print_expr_verilog (FILE *output_stream,
					  const char* expr_set_name,
					  list_t *in_list,
					  iHashtable *inexprmap,
					  iHashtable *inwidthmap,
					  list_t *out_list,
					  list_t *out_expr_name_list,
					  iHashtable *outwidthmap,
					  list_t *expr_list,
					  list_t* hidden_expr_name_list)
{
  listitem_t *li;
  char dummy_char;
  
  list_t *all_names = list_new ();

  _Hexpr = phash_new (8);
  _Hwidth = phash_new (8);
  

  if (!output_stream) fatal_error("ExternalExprOpt::print_expr_verilog: verilog file is not writable");

  fprintf(output_stream,"// generated expression module for %s\n\n\n", expr_set_name);

  // module header
  fprintf(output_stream, "module %s (", expr_set_name);

  // now the ports for the header - first inputs
  bool first = true;


  listitem_t *li_search;
  for (li = list_first (in_list); li; li = list_next (li))
  {
    if (first) first=false; 
    else fprintf(output_stream, ", ");
    std::string current = (char *) ihash_lookup(inexprmap, (long) list_value (li))->v;
    bool skip = false;
    // omit ports with the same name
    for (li_search = list_next (li); li_search; li_search = list_next (li_search))
    {
      std::string search =  (char *) ihash_lookup(inexprmap, (long) list_value (li_search))->v;
      if (current.compare(search) == 0) {
        skip = true;
        break;
      }
    }
    if (!skip) {
      fprintf(output_stream, "%s", current.data());
      list_append (all_names, current.data());
    }
  }
  // now the ports for the header  - than outputs
  listitem_t *li_name = list_first (out_expr_name_list);
  for (li = list_first (out_list); li; li = list_next (li))
  {
    if (first) first=false; 
    else fprintf(output_stream, ", ");
    Assert(li_name, "output name list and output expr list dont have the same length");
    std::string current = (char *) list_value (li_name);
    bool skip = false;
    // omit ports with the same name
    for (li_search = list_next (li_name); li_search; li_search = list_next (li_search))
    {
      std::string search =  (char *) list_value (li_search);
      if (current.compare(search) == 0) {
        skip = true;
        break;
      }
    }
    if (!skip) {
      fprintf(output_stream, "%s", current.data());
      list_append (all_names, current.data());
    }
    li_name = list_next(li_name);
  }
  fprintf(output_stream, " );\n");

  int vectorize = (config_get_int("expropt.vectorize_all_ports") == 0) ? 0 : 1;

  _varwidths.clear();
  // print input ports with bitwidth
  fprintf(output_stream, "\n\t// print input ports with bitwidth\n");
  
  for (li = list_first (in_list); li; li = list_next (li))
  {
    // make sure you dont print a port 2+ times - the tools really dont like that
    std::string current = (char *) ihash_lookup(inexprmap, (long) list_value (li))->v;
    bool skip = false;
    for (li_search = list_next (li); li_search; li_search = list_next (li_search))
    {
      std::string search =  (char *) ihash_lookup(inexprmap, (long) list_value (li_search))->v;
      if (current.compare(search) == 0) {
        skip = true;
        break;
      }
    }
    if (skip) continue;
    // look up the bitwidth
    int width = ihash_lookup(inwidthmap, (long) list_value (li))->i;
    if ( width <=0 ) fatal_error("ExternalExprOpt::print_expr_verilog error: Expression operands have incompatible bit widths\n");
    else if (width == 1 && vectorize==0) fprintf(output_stream, "\tinput %s ;\n", current.data());
    else fprintf(output_stream, "\tinput [%i:0] %s ;\n", width-1, current.data());
    _varwidths[current] = width;
  }

  // print output ports with bitwidth
  fprintf(output_stream, "\n\t// print output ports with bitwidth\n");
  li_name = list_first (out_expr_name_list);
  for (li = list_first (out_list); li; li = list_next (li))
  {
    // make sure you dont print a port 2+ times - the tools really dont like that
    Assert(li_name, "output name list and output expr list dont have the same length");
    std::string current = (char *) list_value (li_name);
    bool skip = false;
    // omit ports with the same name
    for (li_search = list_next (li_name); li_search; li_search = list_next (li_search))
    {
      std::string search =  (char *) list_value (li_search);
      if (current.compare(search) == 0) {
        skip = true;
        break;
      }
    }
    li_name = list_next(li_name);
    if (skip) continue;
    // look up the bitwidth
    int width = ihash_lookup(outwidthmap, (long) list_value (li))->i;
    if ( width <=0 ) fatal_error("chpexpr2verilog::print_expr_set error: Expression operands have incompatible bit widths\n");
    else if (width == 1 && vectorize==0) fprintf(output_stream, "\toutput %s ;\n", current.data());
    else fprintf(output_stream, "\toutput [%i:0] %s ;\n", width-1, current.data());
    _varwidths[current] = width;
  }

  //the hidden logic statements
  if (expr_list != NULL && hidden_expr_name_list != NULL && !list_isempty(expr_list) && !list_isempty(hidden_expr_name_list))
  {
    li_name = list_first (hidden_expr_name_list);
    fprintf(output_stream, "\n\t// the hidden logic vars declare\n");
    for (li = list_first (expr_list); li; li = list_next (li))
    {
      Expr *e = (Expr*) list_value (li);
      Assert(li_name, "output name list and output expr list dont have the same length");
      std::string current = (char *) list_value (li_name);
      bool skip = false;
      // omit ports with the same name
      for (li_search = list_next (li_name); li_search; li_search = list_next (li_search))
      {
        std::string search =  (char *) list_value (li_search);
        if (current.compare(search) == 0) {
          skip = true;
          break;
        }
      }
      li_name = list_next(li_name);
      if (skip) continue;
      // the bitwidth
      int width = ihash_lookup(outwidthmap, (long) list_value (li))->i;
      if ( width <=0 ) fatal_error("chpexpr2verilog::print_expr_set error: Expression operands have incompatible bit widths\n");
      else if (width == 1 && vectorize==0) fprintf(output_stream, "\twire %s ;\n", current.data());
      else fprintf(output_stream, "\twire [%i:0] %s ;\n", width-1, current.data());
      list_append (all_names, current.data());
      _varwidths[current] = width;
    }
  }

  dummy_char = 'a';
  do {
    snprintf (_dummy_prefix, 10, "_xtp%c", dummy_char);
    for (li_name = list_first (all_names); li_name; li_name = list_next (li_name)) {
      if (strncmp ((char *)list_value (li_name), _dummy_prefix,
		   strlen (_dummy_prefix)) == 0) {
	break;
      }
    }
    if (li_name) {
      dummy_char ++;
      if (dummy_char > 'z')
	fatal_error ("Could not find simple unique prefix!");
    }
  } while (li_name);
  _dummy_idx = 0;

  list_free (all_names);

  //the hidden logic statements
  if (expr_list != NULL && hidden_expr_name_list != NULL && !list_isempty(expr_list) && !list_isempty(hidden_expr_name_list))
  {
    li_name = list_first (hidden_expr_name_list);
    fprintf(output_stream, "\n\t// the hidden logic statements as assigns\n");
    for (li = list_first (expr_list); li; li = list_next (li))
    {
      Expr *e = (Expr*) list_value (li);
      Assert(li_name, "output name list and output expr list dont have the same length");
      std::string current = (char *) list_value (li_name);
      bool skip = false;
      // omit ports with the same name
      for (li_search = list_next (li_name); li_search; li_search = list_next (li_search))
      {
        std::string search =  (char *) list_value (li_search);
        if (current.compare(search) == 0) {
          skip = true;
          break;
        }
      }
      li_name = list_next(li_name);
      if (skip) continue;
     // also print the hidden assigns
      int idx = print_expression(output_stream, e, inexprmap);
      char buf[100];
      _gen_dummy_id (buf, 100, idx);
      fprintf(output_stream,"\tassign %s = %s;\n", current.data(), buf);
    }
  }

  //the actuall logic statements
  fprintf(output_stream, "\n\t// the actuall logic statements as assigns\n");
  li_name = list_first (out_expr_name_list);
  for (li = list_first (out_list); li; li = list_next (li))
  {
    Expr *e = (Expr*) list_value (li);
    char buf[100];
    std::string current = (char *) list_value (li_name);
    int idx = print_expression(output_stream, e, inexprmap);
    _gen_dummy_id (buf, 100, idx);
    fprintf(output_stream,"\tassign %s = %s;\n", current.data(), buf);
    li_name = list_next(li_name);
  }
  fprintf(output_stream, "\nendmodule\n");

  phash_free (_Hexpr);
  phash_free (_Hwidth);
}

/*
 * the recusive print method for the act expression data structure. it
 * will use the hashtable expression map to get the IDs for the
 * variables and constants.
 * 
 * for the constants they can be either defined in the exprmap, than
 * they are printed as inputs (for dualrail systems) or if they are
 * not in the exprmap, they will be printed as a constant in verilog
 * for the tool to optimise and map to tiecells (for bundled data)
 * 
 * the keys for the exprmaps are the pointers of the e of type E_VAR
 * and optinal of E_INT, E_TRUE, E_FALSE (or E_REAL) for dualrail.  if
 * a mapping exsists for these leaf types the mapping will be prefered
 * over printing the value.
 */
int ExternalExprOpt::print_expression(FILE *output_stream, Expr *e,
				      iHashtable *exprmap, int *width)
{
  int tmp;
  int lw, rw;
  int lidx, ridx;
  int res, resw;
  char buf[100];
  Expr *orig_e = e;

  phash_bucket_t *b;

  b = phash_lookup (_Hexpr, e);
  if (b) {
    phash_bucket_t *b2;
    // we need to set the bitwidth!
    if (width) {
      b2 = phash_lookup (_Hwidth, e);
      *width = b2->i;
    }
    return b->i;
  }

#define DUMP_DECL_ASSIGN						\
  do {									\
    res = _gen_fresh_idx ();						\
    _gen_dummy_id (buf, 100, res);					\
    if (resw == 1) {							\
      fprintf (output_stream, "\twire %s;\n", buf);			\
    }									\
    else {								\
      fprintf (output_stream, "\twire [%d:0] %s;\n", resw-1, buf);	\
    }									\
    fprintf (output_stream, "\tassign %s = ", buf);			\
    if (width) {							\
      *width = resw;							\
    }									\
  } while (0)

  lw = -1;
  rw = -1;

  switch (e->type) {
  case E_BUILTIN_BOOL:
    lidx = print_expression(output_stream, e->u.e.l, exprmap);

    /* lhs, res has bitwidth 1 */
    resw = 1;
    DUMP_DECL_ASSIGN;

    /* rhs */
    _gen_dummy_id (buf, 100, lidx);
    fprintf (output_stream, "%s ? 1'b1 : 1'b0", buf);
    break;

  case E_BUILTIN_INT:
    lidx = print_expression(output_stream, e->u.e.l, exprmap);
    
    if (!e->u.e.r) {
      resw = 1;
    }
    else {
      resw = e->u.e.r->u.ival.v;
    }
    DUMP_DECL_ASSIGN;

    _gen_dummy_id (buf, 100, lidx);
    fprintf (output_stream, "%s", buf); 
    
    break;

  case (E_QUERY):
    tmp = print_expression (output_stream, e->u.e.l, exprmap);
    lidx = print_expression (output_stream, e->u.e.r->u.e.l, exprmap, &lw);
    ridx = print_expression (output_stream, e->u.e.r->u.e.l, exprmap, &rw);
    resw = act_expr_bitwidth (e->type, lw, rw);

    DUMP_DECL_ASSIGN;
    _gen_dummy_id (buf, 100, tmp);
    fprintf (output_stream, " %s ? ", buf);
    _gen_dummy_id (buf, 100, lidx);
    fprintf (output_stream, " %s : ", buf);
    _gen_dummy_id (buf, 100, ridx);
    fprintf (output_stream, " %s", buf);
    break;
    
    /* no padding needed, binary */
  case (E_LT):
  case (E_GT):
  case (E_LE):
  case (E_GE):
  case (E_EQ):
  case (E_NE):
  case (E_AND):
  case (E_OR):
  case (E_DIV):
  case (E_MOD):
  case (E_LSR):
  case (E_ASR):
  case (E_XOR):
    lidx = print_expression(output_stream, e->u.e.l, exprmap, &lw);
    ridx = print_expression(output_stream, e->u.e.r, exprmap, &rw);

    resw = act_expr_bitwidth (e->type, lw, rw);
    DUMP_DECL_ASSIGN;

    _gen_dummy_id (buf, 100, lidx);
    fprintf(output_stream, "%s ", buf);
    if (e->type == E_AND) {
      fprintf (output_stream, "&");
    }
    else if (e->type == E_OR) {
      fprintf (output_stream, "|");
    }
    else if (e->type == E_XOR) {
      fprintf (output_stream, "^");
    }
    else if (e->type == E_DIV) {
      fprintf (output_stream, "/");
    }      
    else if (e->type == E_MOD) {
      fprintf (output_stream, "%%");
    }
    else if (e->type == E_LSR) {
      fprintf (output_stream, ">>");
    }
    else if (e->type == E_ASR) {
      fprintf (output_stream, ">>>");
    }
    else if (e->type == E_LT) {
      fprintf (output_stream, "<");
    }
    else if (e->type == E_LT) {
      fprintf (output_stream, "<");
    }
    else if (e->type == E_GT) {
      fprintf (output_stream, ">");
    }
    else if (e->type == E_LE) {
      fprintf (output_stream, "<=");
    }
    else if (e->type == E_GE) {
      fprintf (output_stream, ">=");
    }
    else if (e->type == E_EQ) {
      fprintf (output_stream, "==");
    }
    else if (e->type == E_NE) {
      fprintf (output_stream, "!=");
    }
    _gen_dummy_id (buf, 100, ridx);
    fprintf(output_stream, " %s", buf);
    break;

    /* unary */
  case (E_NOT):
  case (E_COMPLEMENT):
  case E_UMINUS:
    lidx = print_expression(output_stream, e->u.e.l, exprmap, &lw);
    resw = act_expr_bitwidth (e->type, lw, rw);
    DUMP_DECL_ASSIGN;

    if (e->type == E_NOT || e->type == E_COMPLEMENT) {
      fprintf(output_stream, "~");
    }
    else if (e->type == E_UMINUS) {
      fprintf(output_stream, "-");
    }
    _gen_dummy_id (buf, 100, lidx);
    fprintf (output_stream, "%s", buf);
    break;

    /* padding needed */
  case (E_PLUS):
  case (E_MINUS):
  case (E_MULT):
  case (E_LSL):
    lidx = print_expression (output_stream, e->u.e.l, exprmap, &lw);
    ridx = print_expression (output_stream, e->u.e.r, exprmap, &rw);
    resw = act_expr_bitwidth (e->type, lw, rw);

    /* pad left */
    DUMP_DECL_ASSIGN;
    _gen_dummy_id (buf, 100, lidx);
    if (lw < resw) {
      fprintf (output_stream, "{%d'b", resw-lw);
      for (int i=0; i < resw-lw; i++) {
	fprintf (output_stream, "0");
      }
      fprintf (output_stream, ",%s};\n", buf);
    }
    else if (lw > resw) {
      fprintf (output_stream, "%s[%d:0]", buf, resw-1);
    }
    else {
      fprintf (output_stream, "%s;\n", buf);
    }
    lidx = res;

    DUMP_DECL_ASSIGN;
    _gen_dummy_id (buf, 100, ridx);
    if (rw < resw) {
      fprintf (output_stream, "{%d'b", resw-rw);
      for (int i=0; i < resw-rw; i++) {
	fprintf (output_stream, "0");
      }
      fprintf (output_stream, ",%s};\n", buf);
    }
    else if (rw > resw) {
      fprintf (output_stream, "%s[%d:0]", buf, resw-1);
    }
    else {
      fprintf (output_stream, "%s;\n", buf);
    }
    ridx = res;
    
    DUMP_DECL_ASSIGN;
    _gen_dummy_id (buf, 100, lidx);
    fprintf(output_stream, "%s ", buf);
    if (e->type == E_PLUS) {
      fprintf (output_stream, "+");
    }
    else if (e->type == E_MINUS) {
      fprintf (output_stream, "-");
    }
    else if (e->type == E_MULT) {
      fprintf (output_stream, "*");
    }
    else if (e->type == E_LSL) {
      fprintf (output_stream, "<<");
    }      
    _gen_dummy_id (buf, 100, ridx);
    fprintf (output_stream, " %s", buf);
    break;

    case (E_INT):
    {
      ihash_bucket_t *b;
      b = ihash_lookup (exprmap, (long)(e));
      if (b) {
	resw = 64;
	DUMP_DECL_ASSIGN;
	fprintf(output_stream, "%s", (char *)b->v);
	warning ("Int bitwidth unspecified");
      }
      else {
	BigInt *bi = (BigInt *) e->u.ival.v_extra;
	if (bi) {
	  resw = bi->getWidth();
	}
	else {
	  resw = act_expr_intwidth (e->u.ival.v);
	}
	DUMP_DECL_ASSIGN;
	if (bi) {
	  fprintf (output_stream, "%d'b", resw);
	  bi->bitPrint (output_stream);
	}
	else {
	  fprintf(output_stream, "%d'h%lx", resw, e->u.ival.v);
	}
      }
    }
    break;
    
    case (E_VAR):
    {
      ihash_bucket_t *b;
      std::string tmp;
      b = ihash_lookup (exprmap, (long)(e));
      Assert (b, "variable not found in variable map");
      tmp = (char *)b->v;
      if (_varwidths.find (tmp) != _varwidths.end()) {
	resw = _varwidths[tmp];
	DUMP_DECL_ASSIGN;
	fprintf (output_stream, "%s", (char *)b->v);
      }
      else {
	fatal_error ("Could not find bitwidth for variable %s!\n", (char *)b->v);
      }
    }
    break;
    
    case (E_LPAR):
      fatal_error("LPAR %u not implemented", e->type);
      break;
    case (E_RPAR):
      fatal_error("RPAR %u not implemented", e->type);
      break;
      
    case (E_TRUE):
    {
      ihash_bucket_t *b;
      resw = 1;
      DUMP_DECL_ASSIGN;
      b = ihash_lookup (exprmap, (long)(e));
      if (b) fprintf(output_stream, "%s", (char *)b->v);
      else fprintf(output_stream, " 1'b1 ");
    }
    break;
    case (E_FALSE):
    {
      ihash_bucket_t *b;
      resw = 1;
      DUMP_DECL_ASSIGN;
      b = ihash_lookup (exprmap, (long)(e));
      if (b) fprintf(output_stream, "%s", (char *)b->v);
      else fprintf(output_stream, " 1'b0 ");
    }
      break;
    case (E_COLON):
      fatal_error ("Should not be here (colon)");
      break;
    case (E_PROBE):
      fatal_error("PROBE %u should have been handled else where", e->type);
      break;
    case (E_COMMA):
      fatal_error("COMMA %u should have been handled else where", e->type);
      break;
    case (E_CONCAT):
      {
	resw = 0;
	list_t *resl = list_new ();

	while (e) {
	  lidx = print_expression (output_stream, e->u.e.l, exprmap, &lw);
	  resw += lw;
	  list_iappend (resl, lidx);
	  e = e->u.e.r;
	}
	DUMP_DECL_ASSIGN;
	fprintf (output_stream, "{");
	for (listitem_t *li = list_first (resl); li; li = list_next (li)) {
	  _gen_dummy_id (buf, 100, list_ivalue (li));
	  fprintf (output_stream, "%s", buf);
	  if (list_next (li)) {
	    fprintf (output_stream, ", ");
	  }
	}
	fprintf (output_stream, "}");
      }
      break;
      
    case (E_BITFIELD):
      unsigned int l;
      unsigned int r;
      if (e->u.e.r->u.e.l) {
         l = (unsigned long) e->u.e.r->u.e.r->u.ival.v;
         r = (unsigned long) e->u.e.r->u.e.l->u.ival.v;
      }
      else {
         l = (unsigned long) e->u.e.r->u.e.r->u.ival.v;
         r = l;
      }
      resw = l - r + 1;
      DUMP_DECL_ASSIGN;
      {
	ihash_bucket_t *b;
	b = ihash_lookup (exprmap, (long)(e));
	Assert (b, "variable not found in variable map");
	fprintf(output_stream, "%s", (char *)b->v);
      }
      fprintf(output_stream, " [");
      if (l!=r) {
        fprintf(output_stream, "%i:", l);
        fprintf(output_stream, "%i", r);
      } else {
        fprintf(output_stream, "%i", r);
      }
      fprintf(output_stream, "]");
      break;

    case (E_REAL):
    {
      fatal_error ("No reals!");
      ihash_bucket_t *b;
      b = ihash_lookup (exprmap, (long)(e));
      if (b) fprintf(output_stream, "%s", (char *)b->v);
      else fprintf(output_stream, "64'd%lu", e->u.ival.v);
    }
      break;
    case (E_RAWFREE):
      fprintf(output_stream, "RAWFREE\n");
      fatal_error("%u should have been handled else where", e->type);
      break;
    case (E_END):
      fprintf(output_stream, "END\n");
      fatal_error("%u should have been handled else where", e->type);
      break;
    case (E_NUMBER):
      fprintf(output_stream, "NUMBER\n");
      fatal_error("%u should have been handled else where", e->type);
      break;
    case (E_FUNCTION):
      fprintf(output_stream, "FUNCTION\n");
      fatal_error("%u should have been handled else where", e->type);
      break;
    default:
      fprintf(output_stream, "Whaaat?! %i\n", e->type);
      break;
  }
  fprintf (output_stream, ";\n");
  b = phash_add (_Hexpr, orig_e);
  b->i = res;
  if (width) {
    b = phash_add (_Hwidth, orig_e);
    b->i = *width;
  }
  return res;
}

/**
 * generate the genus tlc script. should be a seperate file and regulated by a compile flag.
 */
void ExternalExprOpt::generate_genus_tcl(const char *tcl_file_name, const char *file_name, const char *out_file_name, const char* process_name)
{    
    // open the tcl file for writing
    FILE *tcl_file;
    if (tcl_file_name) {
        tcl_file = fopen(tcl_file_name, "w");
        if (!tcl_file) {
        fatal_error ("Could not open file `%s' for writing", tcl_file_name);
        }
    }
    else {
        tcl_file = stdout;
    }

    // the configuration variables for reuse
    char *configreturn = NULL, *configreturn2 = NULL, *configreturn3 = NULL;
    fprintf(tcl_file, "# generated genus run file for %s\n\n", process_name);

    // print the search paths if the files just have filenames not a full path
    configreturn = config_get_string("expropt.searchpath");
    if (strcmp(configreturn,"none") != 0) fprintf(tcl_file, "set_db init_lib_search_path \"%s\"\n",configreturn);
    
    // set settings to high (we jsut do logic syntesis) and force unix program behavior exit on fail
    fprintf(tcl_file, "set_db fail_on_error_mesg true\n");
    fprintf(tcl_file, "set_db syn_generic_effort high \n");
    fprintf(tcl_file, "set_db lp_power_analysis_effort high \n");
    fprintf(tcl_file, "set_db syn_map_effort high \n");
    fprintf(tcl_file, "set_db syn_opt_effort high \n");
    fprintf(tcl_file, "create_library_domain tt_normaltemp \n");

    // the library used for mapping
    configreturn = config_get_string("expropt.liberty_tt_typtemp");
    if (strcmp(configreturn,"none") != 0) fprintf(tcl_file, "set_db library_domain:tt_normaltemp .library { %s } \n", configreturn);
    else fatal_error("please define \"liberty_tt_typtemp\" in expropt configuration file");

    // the lef for area reporting? somehow the liberty file does not do the trick
    configreturn = config_get_string("expropt.lef");
    if (strcmp(configreturn,"none") != 0) fprintf(tcl_file, "set_db lef_library { %s } \n", configreturn);

    // the cap table to get better wireload and drive strength estimations
    configreturn = config_get_string("expropt.captable");
    if (strcmp(configreturn,"none") != 0) fprintf(tcl_file, "set_db cap_table_file { %s } \n", configreturn);

    // read and elaborate
    fprintf(tcl_file, "set_db hdl_array_naming_style %%s\\[%%d\\] \n");
    
    fprintf(tcl_file, "read_hdl \"%s\"\n",file_name);

    fprintf(tcl_file, "elaborate %s\n",process_name);

    // set units for export @TODO use SI units
    fprintf(tcl_file, "set_units -time ps\n");
    fprintf(tcl_file, "set_units -capacitance fF\n");
    fprintf(tcl_file, "set_load_unit -femtofarads\n");
    //fprintf(tcl_file, "set_units -power W\n"); //this one needs genus 19
    fprintf(tcl_file, "set soceSupportWireLoadMode 1\n");
    fprintf(tcl_file, "set_wire_load_model -name auto_select\n");

    // dont know sanity check, works also without
    fprintf(tcl_file, "check_design\n"); // do we need them?
    fprintf(tcl_file, "check_design -unresolved\n"); // do we need them?

    // the timing contraint file for timing reports
    configreturn3 = config_get_string("expropt.timing_constraint_sdf");
    bool delay_min = false, delay_max = false, typ = false, power_max = false;
    
    // dynamicly generate the alaysis corners depending on which files are provided in the configuration.
    if (strcmp(configreturn3,"none") != 0)
    {
      //fprintf(tcl_file, "setenv ENABLE_20NM_FEATURES TRUE\n");
      fprintf(tcl_file, "create_constraint_mode -name CONSTRAINT_MODE_1 -sdc_files { %s }\n", configreturn3);
      int configint = config_get_int("expropt.hightemp");
      configreturn = config_get_string("expropt.liberty_ss_hightemp");
      configreturn2 = config_get_string("expropt.qrc_rcmax");
      if (strcmp(configreturn,"none") != 0 && strcmp(configreturn2,"none") != 0 && configint != 0) {
        fprintf(tcl_file, "create_library_set -name LIB_SET_TIMING_SLOW -timing { %s }\n", configreturn);
        fprintf(tcl_file, "create_timing_condition -name TIMING_COND_SLOW -library_sets LIB_SET_TIMING_SLOW\n");
        fprintf(tcl_file, "create_rc_corner -name RC_CORNER_RCMAX_HIGHTEMP -temperature %d -qrc_tech %s\n", configint, configreturn2);
        fprintf(tcl_file, "create_delay_corner -name CORNER_TIMING_SLOW -timing_condition TIMING_COND_SLOW -rc_corner RC_CORNER_RCMAX_HIGHTEMP\n");
        fprintf(tcl_file, "create_analysis_view -name ANALYSIS_VIEW_TIMING_SLOW -constraint_mode CONSTRAINT_MODE_1 -delay_corner CORNER_TIMING_SLOW\n");
        delay_max = true;
      }

      configint = config_get_int("expropt.lowtemp");
      configreturn = config_get_string("expropt.liberty_ff_lowtemp");
      configreturn2 = config_get_string("expropt.qrc_rcmin");
      if (strcmp(configreturn,"none") != 0 && strcmp(configreturn2,"none") != 0 && configint != 0) {
        fprintf(tcl_file, "create_library_set -name LIB_SET_TIMING_FAST -timing { %s }\n", configreturn);
        fprintf(tcl_file, "create_timing_condition -name TIMING_COND_FAST -library_sets LIB_SET_TIMING_FAST\n");
        fprintf(tcl_file, "create_rc_corner -name RC_CORNER_RCMIN_LOWTEMP -temperature %d -qrc_tech %s\n", configint, configreturn2);
        fprintf(tcl_file, "create_delay_corner -name CORNER_TIMING_FAST -timing_condition TIMING_COND_FAST -rc_corner RC_CORNER_RCMIN_LOWTEMP\n");
        fprintf(tcl_file, "create_analysis_view -name ANALYSIS_VIEW_TIMING_FAST -constraint_mode CONSTRAINT_MODE_1 -delay_corner CORNER_TIMING_FAST\n");
        delay_min = true;
      }
      
      configint = config_get_int("expropt.typtemp");
      configreturn = config_get_string("expropt.liberty_tt_typtemp");
      configreturn2 = config_get_string("expropt.qrc_rctyp");
      if (strcmp(configreturn,"none") != 0 && strcmp(configreturn2,"none") != 0 && configint != 0) {
        fprintf(tcl_file, "create_library_set -name LIB_SET_TYP -timing { %s }\n", configreturn);
        fprintf(tcl_file, "create_timing_condition -name TIMING_COND_TYP -library_sets LIB_SET_TYP\n");
        fprintf(tcl_file, "create_rc_corner -name RC_CORNER_RCTYP_TYPTEMP -temperature %d -qrc_tech %s\n", configint, configreturn2);
        fprintf(tcl_file, "create_delay_corner -name CORNER_TYP -timing_condition TIMING_COND_TYP -rc_corner RC_CORNER_RCTYP_TYPTEMP\n");
        fprintf(tcl_file, "create_analysis_view -name ANALYSIS_VIEW_TYP -constraint_mode CONSTRAINT_MODE_1 -delay_corner CORNER_TYP\n");
        typ = true;
      }
      
      configint = config_get_int("expropt.hightemp");
      configreturn = config_get_string("expropt.liberty_ff_hightemp");
      configreturn2 = config_get_string("expropt.qrc_rcmin");
      if (strcmp(configreturn,"none") != 0 && strcmp(configreturn2,"none") != 0 && configint != 0) {
        fprintf(tcl_file, "create_library_set -name LIB_SET_POWER_MAX -timing { %s }\n", configreturn);
        fprintf(tcl_file, "create_timing_condition -name TIMING_COND_MAX_POWER -library_sets LIB_SET_POWER_MAX\n");
        fprintf(tcl_file, "create_rc_corner -name RC_CORNER_RCMIN_MAXTEMP -temperature %d -qrc_tech %s\n", configint, configreturn2);
        fprintf(tcl_file, "create_delay_corner -name CORNER_POWER_MAX -timing_condition TIMING_COND_MAX_POWER -rc_corner RC_CORNER_RCMIN_MAXTEMP\n");
        fprintf(tcl_file, "create_analysis_view -name ANALYSIS_VIEW_POWER_MAX -constraint_mode CONSTRAINT_MODE_1 -delay_corner CORNER_POWER_MAX\n");
        power_max = true;
      }

      if (delay_min || typ || delay_max || power_max) {
        fprintf(tcl_file, "set_analysis_view");
        fprintf(tcl_file, " -hold { ");
        if (typ) fprintf(tcl_file, " ANALYSIS_VIEW_TYP");
        if (delay_min) fprintf(tcl_file, " ANALYSIS_VIEW_TIMING_FAST");
        fprintf(tcl_file, " }");

        fprintf(tcl_file, " -setup { ");
        if (typ) fprintf(tcl_file, " ANALYSIS_VIEW_TYP");
        if (delay_max) fprintf(tcl_file, " ANALYSIS_VIEW_TIMING_SLOW");
        if (delay_min) fprintf(tcl_file, " ANALYSIS_VIEW_TIMING_FAST");
        if (power_max) fprintf(tcl_file, " ANALYSIS_VIEW_POWER_MAX");
        fprintf(tcl_file, " }");

        if (typ) fprintf(tcl_file, " -leakage ANALYSIS_VIEW_TYP -dynamic ANALYSIS_VIEW_TYP");
        else if (power_max) fprintf(tcl_file, " -leakage ANALYSIS_VIEW_POWER_MAX -dynamic ANALYSIS_VIEW_POWER_MAX");
        fprintf(tcl_file, "\n");
      }


    }

    // start the syntesis

    fprintf(tcl_file, "init_design\n");
    fprintf(tcl_file, "syn_generic\n");
    fprintf(tcl_file, "syn_map\n");
    
    // tiecells insert yes / no
    if ( use_tie_cells ) {
        // the option is to either use one tiehi+tielo cell for all => unique, 
        fprintf(tcl_file, "set_db use_tiehilo_for_const unique\n");
        // or one tie for every input => duplicate, or no => none
        //fprintf(tcl_file, "set_db use_tiehilo_for_const duplicate\n");
    }else{
        fprintf(tcl_file, "set_db use_tiehilo_for_const none\n");
    }

    // optimisation
    fprintf(tcl_file, "syn_opt\n");
    
    // and dynamicly print reports, if files are provided in configuration
    fprintf(tcl_file, "redirect %s.area.log { report_gates; report_area }\n",out_file_name);
    if (typ) fprintf(tcl_file, "redirect %s.timing_typ.log { report_timing -unconstrained -view ANALYSIS_VIEW_TYP }\n",out_file_name );
    else fprintf(tcl_file, "redirect %s.timing_typ.log { report_timing -unconstrained }\n",out_file_name );
    if (delay_min) fprintf(tcl_file, "redirect %s.timing_min.log { report_timing -unconstrained -view ANALYSIS_VIEW_TIMING_FAST }\n",out_file_name );
    if (delay_max) fprintf(tcl_file, "redirect %s.timing_max.log { report_timing -unconstrained -view ANALYSIS_VIEW_TIMING_SLOW }\n",out_file_name );
    if (typ) fprintf(tcl_file, "redirect %s.power_typ.log { report_power -view ANALYSIS_VIEW_TYP }\n",out_file_name );
    else fprintf(tcl_file, "redirect %s.power_typ.log { report_power }\n",out_file_name );
    if (power_max) fprintf(tcl_file, "redirect %s.power_typ.log { report_power -view ANALYSIS_VIEW_POWER_MAX }\n",out_file_name );
    
    // write the result
    fprintf(tcl_file, "write_hdl  > %s\n",out_file_name);

    // write the files for the formal verification
    fprintf(tcl_file, "write_do_lec -revised_design %s -logfile %s.lec.log > %s.lec.do\n",out_file_name,out_file_name,out_file_name);
    fprintf(tcl_file, "exit 0\n");

    // write and close file, if you dont flush the writing is delayed and genus will get a not complete tcl script.
    if (tcl_file_name) 
    { 
        fflush(tcl_file);
        fclose(tcl_file);
    }

}

/*
 * work in progress have to swicht to fscanf for float probably.
 */
ExprBlockInfo* ExternalExprOpt::parse_genus_log(std::string base_file_name) 
{
  std::string regex_power(" Subtotal %*f %*f %*f %lf");
  std::string regex_power_dynamic(" Subtotal %*f %*f %lf %*f");
  std::string regex_power_static(" Subtotal %lf %*f %*f %*f");
  std::string regex_power_old("%*s %*f %*f %*f %lf");
  std::string regex_power_dynamic_old("%*s %*f %*f %lf %*f");
  std::string regex_power_static_old("%*s %*f %lf %*f %*f");
  std::string regex_area("%*s %*f %*f %*f %lf");
  std::string regex_delay(" Data Path:- %lf");

  std::string log_name = base_file_name;
  log_name.append(".power_typ.log");
  double power =  parse_and_return_max(log_name, regex_power, 0);
  double power_static = parse_and_return_max(log_name, regex_power_static, 0);
  double power_dynamic = parse_and_return_max(log_name, regex_power_dynamic, 0);
  if (power == 0) {
    power =  parse_and_return_max(log_name, regex_power_old, 0);
    power_static = parse_and_return_max(log_name, regex_power_static_old, 0);
    power_dynamic = parse_and_return_max(log_name, regex_power_dynamic_old, 0);
    power_static = power_static * 1e-9;
    power_dynamic = power_dynamic * 1e-9;
    power = power * 1e-9;
  }

  log_name = base_file_name;
  log_name.append(".power_max.log");
  double power_max =  parse_and_return_max(log_name, regex_power, 0);
  double power_max_static = parse_and_return_max(log_name, regex_power_static, 0);
  double power_max_dynamic = parse_and_return_max(log_name, regex_power_dynamic, 0);
  if (power_max == 0) 
  {
    power_max =  parse_and_return_max(log_name, regex_power_old, 0);
    power_max_static = parse_and_return_max(log_name, regex_power_static_old, 0);
    power_max_dynamic = parse_and_return_max(log_name, regex_power_dynamic_old, 0);
    power_max_static = power_max_static * 1e-9;
    power_max_dynamic = power_max_dynamic * 1e-9;
    power_max = power_max * 1e-9;
  }

  // we assume um^2 and we convert to m^2, depends on the liberty file unit
  log_name = base_file_name;
  log_name.append(".area.log");
  double area =  parse_and_return_max(log_name, regex_area, 0);
  area = area * 1e-12;

  // we assume ps as set for in genus tcl
  log_name = base_file_name;
  log_name.append(".timing_min.log");
  double delay_min=  parse_and_return_max(log_name, regex_delay, 0);
  delay_min = delay_min * 1e-12;

  log_name = base_file_name;
  log_name.append(".timing_max.log");
  double delay_max =  parse_and_return_max(log_name, regex_delay, 0);
  delay_max = delay_max * 1e-12;

  log_name = base_file_name;
  log_name.append(".timing_typ.log");
  double delay =  parse_and_return_max(log_name, regex_delay, 0);
  delay = delay * 1e-12;

  return new ExprBlockInfo(delay,delay_min,delay_max,power,power_max,area,power_static,power_dynamic,power_max_static,power_max_dynamic);
}

double ExternalExprOpt::parse_and_return_max(std::string filename, std::string parse_format, double failure_value, bool fail_if_file_does_no_exist)
{ 
  std::ifstream log_stream;
  log_stream.open(filename);
  if (!log_stream.is_open())
  {
    if (fail_if_file_does_no_exist) fatal_error("could not open file %s for reading", filename.data());
    return failure_value;
  } 
  std::string current_line = ""; 
  double return_value = failure_value;
  double value_buffer = 0;
  bool first = true;
  while (std::getline(log_stream, current_line))
  {
    if (sscanf(current_line.data(),parse_format.data(), &value_buffer) == 1)
    {
      if (first)
      {
        return_value = value_buffer;
        first = false;
      }
      else if (value_buffer > return_value) return_value = value_buffer;
    }
  }
  log_stream.close();
  return return_value;
}

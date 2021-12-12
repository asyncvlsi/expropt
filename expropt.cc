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

#include <string.h>

/**
 * Destroy the External Expr Opt:: External Expr Opt object
 * 
 * no memory allocation on global vars, nothing to be done
 */
ExternalExprOpt::~ExternalExprOpt()
{
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

static double parse_abc_info (const char *file)
{
  char buf[10240];
  FILE *fp;
  double ret;

  snprintf (buf, 10240, "%s.log", file);
  fp = fopen (buf, "r");
  if (!fp) {
    return -1;
  }
  while (fgets (buf, 10240, fp)) {
    if (strncmp (buf, "ABC:", 4) == 0) {
      char *tmp = strstr (buf, "Delay =");
      if (tmp) {
	if (sscanf (tmp, "Delay = %lf ps", &ret) == 1) {
	  fclose (fp);
	  return ret*1e-12;
	}
      }
    }
  }
  return -1;
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
  std::string verilog_file = ".";
  verilog_file.append("/exprop_");
  verilog_file.append(expr_set_name);
  std::string mapped_file = verilog_file.data();
  verilog_file.append(".v");
  mapped_file.append("_mapped.v");
  char cmd[4096] = "";
  FILE *verilog_stream;

  std::string sdc_file = ".";
  sdc_file.append("/exprop_");
  sdc_file.append(expr_set_name);
  sdc_file.append(".sdc");

  // open temp verilog file to be syntesised

  verilog_stream = fopen(verilog_file.data(), "w");
  if (!verilog_stream) fatal_error("ExternalExprOpt::run_external_opt: verilog file %s is not writable",verilog_file.data());

  // generate verilog module

  print_expr_verilog(verilog_stream, expr_set_name, in_expr_list, in_expr_map, in_width_map, out_expr_list, out_expr_name_list, out_width_map, hidden_expr_list, hidden_expr_name_list);

  // force write and close file

  fflush(verilog_stream);
  fclose(verilog_stream);

  // generate the exec command for the sysntesis tool and run the syntesis
  int exec_failure = 1;

#ifdef FOUND_exproptcommercial
  ExprOptCommercialHelper *helper = new ExprOptCommercialHelper();
#endif

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
    char* configreturn = config_get_string("expropt.liberty_tt_typtemp");
    if (std::strcmp(configreturn,"none") != 0)
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
  default:
    info =  new ExprBlockInfo(parse_abc_info (mapped_file.data()),
			      0, 0, 0, 0, 0, 0, 0, 0, 0);
    break;
  }

  // clean up temporary files
  if (cleanup) {
    switch (mapper)
    {
    case genus:
      sprintf(cmd,"rm %s && rm %s && rm %s.* && rm %s.* && rm -r fv* && rm -r rtl_fv* && rm genus.*", mapped_file.data(), verilog_file.data(), mapped_file.data(), verilog_file.data());
      break;
    case synopsis:
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
  return info;
}

/*
 * print the verilog module with header, in and outputs. call the expression print method for the assigns rhs.
 */
void ExternalExprOpt::print_expr_verilog (FILE *output_stream, const char* expr_set_name, list_t *in_list,  iHashtable *inexprmap, iHashtable *inwidthmap, list_t *out_list, list_t *out_expr_name_list, iHashtable *outwidthmap, list_t *expr_list, list_t* hidden_expr_name_list)
{
  listitem_t *li;

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
    if (!skip) fprintf(output_stream, "%s", current.data());
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
    if (!skip) fprintf(output_stream, "%s", current.data());
    li_name = list_next(li_name);
  }
  fprintf(output_stream, " );\n");
  
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
    else if (width == 1) fprintf(output_stream, "\tinput %s ;\n", current.data());
    else fprintf(output_stream, "\tinput [%i:0] %s ;\n", width-1, current.data());
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
    else if (width == 1) fprintf(output_stream, "\toutput %s ;\n", current.data());
    else fprintf(output_stream, "\toutput [%i:0] %s ;\n", width-1, current.data());
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
      else if (width == 1) fprintf(output_stream, "\twire %s ;\n", current.data());
      else fprintf(output_stream, "\twire [%i:0] %s ;\n", width-1, current.data());
    }
  }

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
      fprintf(output_stream,"\tassign %s = ", current.data());
      print_expression(output_stream, e, inexprmap);
      fprintf(output_stream,";\n");
    }
  }

  //the actuall logic statements
  fprintf(output_stream, "\n\t// the actuall logic statements as assigns\n");
  li_name = list_first (out_expr_name_list);
  for (li = list_first (out_list); li; li = list_next (li))
  {
    Expr *e = (Expr*) list_value (li);
    std::string current = (char *) list_value (li_name);
    fprintf(output_stream,"\tassign %s = ", current.data());
    print_expression(output_stream, e, inexprmap);
    fprintf(output_stream,";\n");
    li_name = list_next(li_name);
  }
  fprintf(output_stream, "\nendmodule\n");  
}

/*
 * the recusive print method for the act expression data structure. it will use the hashtable expression map to get the IDs for the variables and constants.
 * 
 * for the constants they can be either defined in the exprmap, than they are printed as inputs (for dualrail systems) or if they are not in the exprmap, 
 * they will be printed as a constant in verilog for the tool to optimise and map to tiecells (for bundled data)
 * 
 * the keys for the exprmaps are the pointers of the e of type E_VAR and optinal of E_INT, E_TRUE, E_FALSE (or E_REAL) for dualrail.
 * if a mapping exsists for these leaf types the mapping will be prefered over printing the value.
 */
void ExternalExprOpt::print_expression(FILE *output_stream, Expr *e, iHashtable *exprmap) {
  fprintf (output_stream, "(");
  switch (e->type) {
  case E_BUILTIN_BOOL:
    print_expression(output_stream, e->u.e.l, exprmap);
    fprintf (output_stream, " == 0 ? 1'b0 : 1'b1");
    break;

  case E_BUILTIN_INT:
    if (!e->u.e.r) {
      print_expression(output_stream, e->u.e.l, exprmap);
    }
    else {
      int v;
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf (output_stream, " & ");
      v = e->u.e.r->u.v;
      fprintf (output_stream, "%d'b", v);
      for (int i=0; i < v; i++) {
	fprintf (output_stream, "1");
      }
    }
    break;
    
    case (E_AND):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " & ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_OR):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " | ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_NOT):
      fprintf(output_stream, " ~");
      print_expression(output_stream, e->u.e.l, exprmap);
      break;
    case (E_PLUS):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " + ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_MINUS):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " - ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_MULT):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " * ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_DIV):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " / ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_MOD):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " %% ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_LSL):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " << ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_LSR):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " >> ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_ASR):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " >>> ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_UMINUS):
      fprintf(output_stream, " (-");
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, ")");
      break;
    case (E_INT):
    {
      ihash_bucket_t *b;
      b = ihash_lookup (exprmap, (long)(e));
      if (b) fprintf(output_stream, "%s", (char *)b->v);
      else {
        fprintf(output_stream, "64'd%lu", e->u.v);
      }
    }
      
      break;
    case (E_VAR):
    {
      ihash_bucket_t *b;
      b = ihash_lookup (exprmap, (long)(e));
      Assert (b, "variable not found in variable map");
      fprintf(output_stream, "%s", (char *)b->v);
    }
      break;
    case (E_QUERY):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " ? ");
      print_expression(output_stream, e->u.e.r->u.e.l, exprmap);
      fprintf(output_stream, " : ");
      print_expression(output_stream, e->u.e.r->u.e.r, exprmap);
      break;
    case (E_LPAR):
      fprintf(output_stream, "LPAR\n");
      fatal_error("%u not implemented", e->type);
      break;
    case (E_RPAR):
      fprintf(output_stream, "RPAR\n");
      fatal_error("%u not implemented", e->type);
      break;
    case (E_XOR):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " ^ ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_LT):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " < ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_GT):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " > ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_LE):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " <=");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_GE):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " >= ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_EQ):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " == ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_NE):
      print_expression(output_stream, e->u.e.l, exprmap);
      fprintf(output_stream, " != ");
      print_expression(output_stream, e->u.e.r, exprmap);
      break;
    case (E_TRUE):
    {
      ihash_bucket_t *b;
      b = ihash_lookup (exprmap, (long)(e));
      if (b) fprintf(output_stream, "%s", (char *)b->v);
      else fprintf(output_stream, " 1'b1 ");
    }
      break;
    case (E_FALSE):
    {
      ihash_bucket_t *b;
      b = ihash_lookup (exprmap, (long)(e));
      if (b) fprintf(output_stream, "%s", (char *)b->v);
      else fprintf(output_stream, " 1'b0 ");
    }
      break;
    case (E_COLON):
      fprintf(output_stream, " : ");
      break;
    case (E_PROBE):
      fprintf(output_stream, "PROBE");
      fatal_error("%u should have been handled else where", e->type);
      break;
    case (E_COMMA):
      fprintf(output_stream, "COMMA");
      fatal_error("%u should have been handled else where", e->type);
      break;
    case (E_CONCAT):
      if (!e->u.e.r) {
	print_expression(output_stream, e->u.e.l, exprmap);
      }
      else {
	Expr *tmp = e;
	fprintf (output_stream, "{");
	while (tmp) {
	  print_expression(output_stream, tmp->u.e.l, exprmap);
	  if (tmp->u.e.r) {
	    fprintf(output_stream, " ,");
	  }
	  tmp = tmp->u.e.r;
	}
	fprintf (output_stream, "}");
      }
      break;
    case (E_BITFIELD):
      unsigned int l;
      unsigned int r;
      if (e->u.e.r->u.e.l) {
         l = (unsigned long) e->u.e.r->u.e.r->u.v;
         r = (unsigned long) e->u.e.r->u.e.l->u.v;
      }
      else {
         l = (unsigned long) e->u.e.r->u.e.r->u.v;
         r = l;
      }

      {
	ihash_bucket_t *b;
	b = ihash_lookup (exprmap, (long)(e));
	Assert (b, "variable not found in variable map");
	fprintf(output_stream, "%s", (char *)b->v);
      }
#if 0      
      fprintf(output_stream, "\\");
      ((ActId *) e->u.e.l)->Print(output_stream);
#endif      
      fprintf(output_stream, " [");
      if (l!=r) {
        fprintf(output_stream, "%i:", l);
        fprintf(output_stream, "%i", r);
      } else {
        fprintf(output_stream, "%i", r);
      }
      fprintf(output_stream, "]");
      break;
    case (E_COMPLEMENT):
      fprintf(output_stream, " ~");
      print_expression(output_stream, e->u.e.l, exprmap);
      break;
    case (E_REAL):
    {
      ihash_bucket_t *b;
      b = ihash_lookup (exprmap, (long)(e));
      if (b) fprintf(output_stream, "%s", (char *)b->v);
      else fprintf(output_stream, "64'd%lu", e->u.v);
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
  fprintf (output_stream, ")");
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
    if (std::strcmp(configreturn,"none") != 0) fprintf(tcl_file, "set_db init_lib_search_path \"%s\"\n",configreturn);
    
    // set settings to high (we jsut do logic syntesis) and force unix program behavior exit on fail
    fprintf(tcl_file, "set_db fail_on_error_mesg true\n");
    fprintf(tcl_file, "set_db syn_generic_effort high \n");
    fprintf(tcl_file, "set_db lp_power_analysis_effort high \n");
    fprintf(tcl_file, "set_db syn_map_effort high \n");
    fprintf(tcl_file, "set_db syn_opt_effort high \n");
    fprintf(tcl_file, "create_library_domain tt_normaltemp \n");

    // the library used for mapping
    configreturn = config_get_string("expropt.liberty_tt_typtemp");
    if (std::strcmp(configreturn,"none") != 0) fprintf(tcl_file, "set_db library_domain:tt_normaltemp .library { %s } \n", configreturn);
    else fatal_error("please define \"liberty_tt_typtemp\" in expropt configuration file");

    // the lef for area reporting? somehow the liberty file does not do the trick
    configreturn = config_get_string("expropt.lef");
    if (std::strcmp(configreturn,"none") != 0) fprintf(tcl_file, "set_db lef_library { %s } \n", configreturn);

    // the cap table to get better wireload and drive strength estimations
    configreturn = config_get_string("expropt.captable");
    if (std::strcmp(configreturn,"none") != 0) fprintf(tcl_file, "set_db cap_table_file { %s } \n", configreturn);

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
    if (std::strcmp(configreturn3,"none") != 0)
    {
      //fprintf(tcl_file, "setenv ENABLE_20NM_FEATURES TRUE\n");
      fprintf(tcl_file, "create_constraint_mode -name CONSTRAINT_MODE_1 -sdc_files { %s }\n", configreturn3);
      int configint = config_get_int("expropt.hightemp");
      configreturn = config_get_string("expropt.liberty_ss_hightemp");
      configreturn2 = config_get_string("expropt.qrc_rcmax");
      if (std::strcmp(configreturn,"none") != 0 && std::strcmp(configreturn2,"none") != 0 && configint != 0) {
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
      if (std::strcmp(configreturn,"none") != 0 && std::strcmp(configreturn2,"none") != 0 && configint != 0) {
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
      if (std::strcmp(configreturn,"none") != 0 && std::strcmp(configreturn2,"none") != 0 && configint != 0) {
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
      if (std::strcmp(configreturn,"none") != 0 && std::strcmp(configreturn2,"none") != 0 && configint != 0) {
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

/*************************************************************************
 *
 *  Copyright (c) 2024 Rajit Manohar
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
#include "expropt.h"
#include <string.h>
#include <stdio.h>


static double parse_yosys_info (std::string file, double *area)
{
  FILE *fp;
  double ret;

  std::string logfile = file + ".log";
  fp = fopen (logfile.c_str(), "r");
  if (!fp) {
    return -1;
  }

  ret = -1;

  if (area) {
    *area = 0;
  }

  char buf[char_buf_sz];
  while (fgets (buf, char_buf_sz, fp)) {
    if (strncmp (buf, "ABC:", 4) == 0) {
      char *tmp = strstr (buf, "Delay =");
      if (tmp) {
	if (sscanf (tmp, "Delay = %lf ps", &ret) == 1) {
	  ret = ret*1e-12;
	}
      }
    // abc's 'stime -p' line also reports the total area
    tmp = strstr (buf,"Area =");
    if(tmp && area){
        if(sscanf (tmp,"Area = %lf",area) ==1) {
            *area = (*area)*1e-12;
        }
      }
    }
  }
  fclose (fp);
  return ret;
}


extern "C"
bool yosys_run (act_syn_info *s)
{
  FILE *fp;
  int len;
  int exec_failure;

  if (!s) {
    warning ("yosys_run: NULL argument!");
    return false;
  }

  len = s->v_in.size();
  // if (s->v_in) {
  //   len = strlen (s->v_in.c_str());
  // }
  // else {
  //   len = 0;
  // }

  if (len < 2 || s->v_in[len-1] != 'v' || s->v_in[len-2] != '.') {
    warning ("yosys_run: Verilog source should end in .v");
    return false;
  }
  
  std::string sdc_file = s->v_in;
  sdc_file.pop_back();
  sdc_file.pop_back();
  sdc_file.append(".sdc");

  fp = fopen (sdc_file.c_str(), "w");
  if (!fp) {
    warning ("Could not open `%s' file!", sdc_file.c_str());
    return false;
  }
  fprintf (fp, "set_load %g\n", config_get_real ("synth.expropt.default_load"));
  fclose (fp);

  // yosys gets its script passed via stdin (very short)
  std::string libfile = config_get_string("synth.liberty.typical");

  std::string cmd = "";
  
  if (libfile!="none") {
    int constr = 0;
    int pos;
    if (config_exists ("synth.expropt.abc.use_constraints")) {
      if (config_get_int ("synth.expropt.abc.use_constraints") == 1) {
	constr = 1;
      }
    }

    // start of the script
    cmd =  cmd + "echo \"read_verilog " + s->v_in + "; synth -noabc -top  " + s->toplevel + ";";

    // tech map
    if (constr) {
      cmd = cmd + " abc -constr " + sdc_file + " -liberty " + libfile + ";"; 
    }
    else {
      cmd = cmd + " abc -liberty " + libfile + ";";
    }

    // tie cells
    if (s->use_tie_cells) {
      cmd = cmd + " hilomap -hicell TIEHIX1 Y -locell TIELOX1 Y -singleton; ";
    }

    // write results
    cmd = cmd + " write_verilog -nohex -nodec " + s->v_out + ";\" | yosys > " + s->v_out + ".log";
  }
  else {
    warning ("Please define \"liberty.typical\" in expropt configuration file");
    return false;
  }
  
  if (config_get_int("synth.expropt.verbose") == 2) {
    printf("running: %s \n", cmd.c_str());
  }
  else if (config_get_int("synth.expropt.verbose") == 1) {
    printf(".");
    fflush(stdout);
  }
  exec_failure = system(cmd.c_str());
  if (exec_failure != 0) {
    fprintf (stderr, "ERROR: command `%s' failed.\n", cmd.c_str());
    return false;
  }
  return true;
}


extern "C"
double yosys_get_metric (act_syn_info *s, expropt_metadata type)
{
  if (type == metadata_area || type == metadata_delay_typ) {
    double res, area;

    res = parse_yosys_info (s->v_out.c_str(), &area);
    if (type == metadata_area) {
      if (!config_exists ("synth.expropt.abc.use_constraints") ||
	  !(config_get_int ("synth.expropt.abc.use_constraints") == 1)) {
	return -1.0;
      }
      return area;
    }
    else {
      return res;
    }
  }
  else {
    return 0.0;
  }
}

extern "C"
void yosys_cleanup (act_syn_info *s)
{
  std::string sdc_file = s->v_in;
  sdc_file.pop_back();
  sdc_file.pop_back();
  sdc_file.append(".sdc");
  
  std::string cmd = "rm " + s->v_out + " && rm " + s->v_in + " && rm " + sdc_file + " && rm " + s->v_out +".* ";

  if (config_get_int("synth.expropt.verbose") == 2) {
    printf("running: %s \n", cmd.c_str());
  }
  else if (config_get_int("synth.expropt.verbose") == 1) {
    printf(".");
    fflush(stdout);
  }
  system(cmd.c_str());
}

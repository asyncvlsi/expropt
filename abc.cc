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
#include "abc_api.h"

extern "C"
bool abc_run (act_syn_info *s)
{
  AbcApi *api;
  
  if (config_get_int("synth.expropt.verbose") == 2) {
    printf("running: built-in abc \n");
  }
  else if (config_get_int("synth.expropt.verbose") == 1) {
    printf(".");
    fflush(stdout);
  }

  std::string sdc_file = s->v_in;
  sdc_file.pop_back();
  sdc_file.pop_back();
  sdc_file.append(".sdc");

  FILE *fp = fopen (sdc_file.c_str(), "w");
  if (!fp) {
    fatal_error ("Could not open `%s' file!", sdc_file.c_str());
  }
  fprintf (fp, "set_load %g\n", config_get_real ("synth.expropt.default_load"));
  if (config_exists("synth.expropt.driving_cell")) {
    fprintf (fp, "set_driving_cell %s\n", config_get_string ("synth.expropt.driving_cell"));
  }
  fclose (fp);
  // FREE (sdc_file);

  api = (AbcApi *) s->space;
  
  if (!api->startSession (s->v_in.c_str(), s->v_out.c_str(), s->toplevel.c_str())) {
    fatal_error ("Unable to start ABC session!");
  }

  if (!api->stdSynthesis ()) {
    fatal_error ("Unable to run logic synthesis using ABC api");
  }

  if (config_exists ("synth.expropt.abc.use_constraints")) {
    if (config_get_int ("synth.expropt.abc.use_constraints") == 1) {
      if (!api->runTiming()) {
	fatal_error ("Unable to run timing");
      }
    }
  }
  if (!api->endSession ()) {
    fatal_error ("Unable to end session with ABC");
  }
  return true;
}


static double parse_abc_info (std::string file, double *area)
{
  FILE *fp;
  double ret;

  std::string logfile = file + ".log";
  fp = fopen (logfile.c_str(), "r");
  if (!fp) {
    if (area) {
      *area = 0;
    }
    return -1;
  }

  ret = -1;

  if (area) {
    *area = 0;
  }

  char buf[char_buf_sz];
  while (fgets (buf, char_buf_sz, fp)) {
    char *tmp = strstr (buf, "Delay =");
    if (tmp) {
      if (sscanf (tmp, "Delay = %lf ps", &ret) == 1) {
	ret = ret*1e-12;
      }
    }
    // we need to parse cells for area!
    else {
      tmp = strstr (buf, "Fanin =");
      if (tmp) {
	char cellname[char_buf_sz];
	int inst;
	double tot_area;
	if (sscanf (buf, "%s Fanin = %*d Instance = %d Area = %lg",
		    cellname, &inst, &tot_area) == 3) {
	  // um^2
	  tot_area *= 1e-12;
	  if (area) {
	    *area += tot_area;
	  }
	}
      }
    }
  }
  fclose (fp);
  return ret;
}


extern "C"
double abc_get_metric (act_syn_info *s, expropt_metadata type)
{
  if (type == metadata_area || type == metadata_delay_typ) {
    double res, area;
    res = parse_abc_info (s->v_out, &area);
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
void abc_cleanup (act_syn_info *s)
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

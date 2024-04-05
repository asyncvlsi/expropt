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

static double parse_yosys_info (const char *file, double *area)
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

  if (area) {
    *area = 0;
  }

  while (fgets (buf, 10240, fp)) {
    if (strncmp (buf, "ABC:", 4) == 0) {
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
    // in um^2, so SI units
    *area = *area * 1e-12;
  }
  fclose (fp);
  return ret;
}


extern "C"
bool yosys_run (act_syn_info *s)
{
  FILE *fp;
  char *sdc_file;
  int len;
  int exec_failure;

  if (!s) {
    warning ("yosys_run: NULL argument!");
    return false;
  }

  if (s->v_in) {
    len = strlen (s->v_in);
  }
  else {
    len = 0;
  }

  if (len < 2 || s->v_in[len-1] != 'v' || s->v_in[len-2] != '.') {
    warning ("yosys_run: Verilog source should end in .v");
    return false;
  }
  
  MALLOC (sdc_file, char, len+3);
  snprintf (sdc_file, len + 3, "%s", s->v_in);
  snprintf (sdc_file + len - 2, 5, ".sdc");

  fp = fopen (sdc_file, "w");
  if (!fp) {
    fatal_error ("Could not open `%s' file!", sdc_file);
  }
  fprintf (fp, "set_load %g\n", config_get_real ("expropt.default_load"));
  fclose (fp);

  // yosys gets its script passed via stdin (very short)
  char *libfile = config_get_string("expropt.liberty_tt_typtemp");

  char cmd[10240];
  
  if (strcmp(libfile,"none") != 0) {
    int constr = 0;
    int pos;
    if (config_exists ("expropt.abc_use_constraints")) {
      if (config_get_int ("expropt.abc_use_constraints") == 1) {
	constr = 1;
      }
    }

    // start of the script
    pos = 0;
    snprintf (cmd + pos, 10240 - pos, 
	      "echo \"read_verilog %s; "
	      "synth -noabc -top %s; ",
	      s->v_in, s->toplevel);
    pos += strlen (cmd + pos);

    // tech map
    if (constr) {
      snprintf (cmd + pos, 10240 - pos, "abc -constr %s -liberty %s; ",
		sdc_file, libfile);
    }
    else {
      snprintf (cmd + pos, 10240 - pos, "abc -liberty %s; ", libfile);
    }
    pos += strlen (cmd + pos);

    // tie cells
    if (s->use_tie_cells) {
      snprintf (cmd + pos, 10240 - pos, 
		"hilomap -hicell TIEHIX1 Y -locell TIELOX1 Y -singleton; ");
      pos += strlen (cmd + pos);
    }

    // write results
    snprintf(cmd + pos, 10240 - pos,
	     "write_verilog -nohex -nodec %s;\" | yosys > %s.log",
	     s->v_out, s->v_out);
  }
  else {
    fatal_error("Please define \"liberty_tt_typtemp\" in expropt configuration file");
  }

  FREE (sdc_file);
  
  if (config_get_int("expropt.verbose") == 2) {
    printf("running: %s \n", cmd);
  }
  else if (config_get_int("expropt.verbose") == 1) {
    printf(".");
    fflush(stdout);
  }
  exec_failure = system(cmd);
  if (exec_failure != 0) {
    fprintf (stderr, "ERROR: command `%s' failed.\n", cmd);
    return false;
  }
  return true;
}


extern "C"
double yosys_get_metric (act_syn_info *s, expropt_metadata type)
{
  if (type == metadata_area || type == metadata_delay_typ) {
    double res, area;

    res = parse_yosys_info (s->v_out, &area);
    if (type == metadata_area) {
      if (!config_exists ("expropt.abc_use_constraints") ||
	  !(config_get_int ("expropt.abc_use_constraints") == 1)) {
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
  char *sdc_file;
  int len;
  char cmd[4096];

  len = strlen (s->v_in);
  MALLOC (sdc_file, char, len+3);
  snprintf (sdc_file, len + 3, "%s", s->v_in);
  snprintf (sdc_file + len - 2, 5, ".sdc");
  
  snprintf(cmd, 4096, "rm %s && rm %s && rm %s && rm %s.* ",
	   s->v_out, s->v_in, sdc_file, s->v_out);

  if (config_get_int("expropt.verbose") == 2) {
    printf("running: %s \n", cmd);
  }
  else if (config_get_int("expropt.verbose") == 1) {
    printf(".");
    fflush(stdout);
  }
  system(cmd);
  FREE (sdc_file);
}

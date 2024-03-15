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
#include "config_pkg.h"
#include <string.h>
#include <stdio.h>

#ifdef FOUND_exproptcommercial
#include <act/exproptcommercial.h>
#endif

extern "C"
bool genus_run (act_syn_info *s)
{
#ifdef FOUND_exproptcommercial
  ExprOptCommercialHelper *helper = new ExprOptCommercialHelper();
  
  s->space = helper;

  int exec_failure = helper->run_genus(s->v_in, s->v_out, s->toplevel);
  
  if (exec_failure != 0) {
    return false;
  }
  
#else
  fatal_error ("cadence genus support missing");
#endif
  
  return true;
}


extern "C"
double genus_get_metric (act_syn_info *s, expropt_metadata type)
{
#ifdef FOUND_exproptcommercial
  ExprOptCommercialHelper *helper = (ExprOptCommercialHelper *) s->space;
  std::string genus_log = s->v_out;
  return helper->parse_genus_log (genus_log, type);
#else
  fatal_error("cadence genus support was not enabled on compile time");
  return 0.0;
#endif
}

extern "C"
void genus_cleanup (act_syn_info *s)
{
  char cmd[4096];

  std::string lec_out = ".";
  lec_out.append ("/noneq.*.");
  lec_out.append (s->toplevel);
  lec_out.append (".*");
  
  snprintf(cmd, 4096, "rm %s && rm %s && rm %s.* && rm %s.* && rm -r fv* && rm -r rtl_fv* && rm %s && rm genus.*",
	   s->v_out, s->v_in,
	   s->v_out, s->v_in,
	   lec_out.data());

  system (cmd);
}

/*************************************************************************
 *
 *  This file is part of act expropt
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
#include <common/int.h>
#include <string.h>

/*
 * print the verilog module with header, in and outputs. call the
 * expression print method for the assigns rhs.
 */
void ExternalExprOpt::print_expr_verilog (FILE *output_stream,
					  std::string expr_set_name,
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
  

  if (!output_stream) {
    fatal_error("ExternalExprOpt::print_expr_verilog: "
		"verilog file is not writable");
  }

  fprintf(output_stream,"// generated expression module for %s\n\n\n", expr_set_name.c_str());

  // module header
  fprintf(output_stream, "module %s (", expr_set_name.c_str());

  // now the ports for the header - first inputs
  bool first = true;

  
  listitem_t *li_search;
  struct Hashtable *repeats;
  repeats = hash_new (4);
  for (li = list_first (in_list); li; li = list_next (li)) {
    if (first) {
      first=false;
    }
    else {
      fprintf(output_stream, ", ");
    }
    std::string current =
      (char *) ihash_lookup(inexprmap, (long) list_value (li))->v;
    
    bool skip = false;

    if (hash_lookup (repeats, current.c_str())) {
      skip = true;
    }
    else {
      hash_add (repeats, current.c_str());
    }
    
    if (!skip) {
      fprintf(output_stream, "%s", current.c_str());
      list_append (all_names, current.c_str());
    }
  }
  hash_free (repeats);

  // ... then outputs
  listitem_t *li_name = list_first (out_expr_name_list);
  repeats = hash_new (4);
  for (li = list_first (out_list); li; li = list_next (li)) {
    if (first) {
      first=false;
    }
    else {
      fprintf(output_stream, ", ");
    }
    Assert(li_name, "output name list and output expr list dont have the same length");
    std::string current = (char *) list_value (li_name);
    bool skip = false;
    if (hash_lookup (repeats, current.c_str())) {
      skip = true;
    }
    else {
      hash_add (repeats, current.c_str());
    }
    if (!skip) {
      fprintf(output_stream, "%s", current.c_str());
      list_append (all_names, current.c_str());
    }
    li_name = list_next(li_name);
  }
  fprintf(output_stream, " );\n");
  hash_free (repeats);

  int vectorize = (config_get_int("synth.expropt.vectorize_all_ports") == 0) ? 0 : 1;

  _varwidths.clear();
  // print input ports with bitwidth
  fprintf(output_stream, "\n\t// print input ports with bitwidth\n");

  repeats = hash_new (4);
  for (li = list_first (in_list); li; li = list_next (li)) {
    // make sure you dont print a port 2+ times - the tools really dont like that
    std::string current = (char *) ihash_lookup(inexprmap, (long) list_value (li))->v;
    bool skip = false;
    if (hash_lookup (repeats, current.c_str())) {
      skip = true;
    }
    else {
      hash_add (repeats, current.c_str());
    }
    if (skip) continue;
    // look up the bitwidth
    int width = ihash_lookup(inwidthmap, (long) list_value (li))->i;
    if ( width <=0 ) fatal_error("ExternalExprOpt::print_expr_verilog error: Expression operands have incompatible bit widths\n");
    else if (width == 1 && vectorize==0) fprintf(output_stream, "\tinput %s ;\n", current.c_str());
    else fprintf(output_stream, "\tinput [%i:0] %s ;\n", width-1, current.c_str());
    _varwidths[current] = width;
  }
  hash_free (repeats);

  // print output ports with bitwidth
  fprintf(output_stream, "\n\t// print output ports with bitwidth\n");
  li_name = list_first (out_expr_name_list);
  repeats = hash_new (4);
  for (li = list_first (out_list); li; li = list_next (li))
  {
    // make sure you dont print a port 2+ times - the tools really dont like that
    Assert(li_name, "output name list and output expr list dont have the same length");
    std::string current = (char *) list_value (li_name);
    bool skip = false;
    
    if (hash_lookup (repeats, current.c_str())) {
      skip = true;
    }
    else {
      hash_add (repeats, current.c_str());
    }
    li_name = list_next(li_name);
    if (skip) continue;
    // look up the bitwidth
    int width = ihash_lookup(outwidthmap, (long) list_value (li))->i;
    if ( width <=0 ) fatal_error("chpexpr2verilog::print_expr_set error: Expression operands have incompatible bit widths\n");
    else if (width == 1 && vectorize==0) fprintf(output_stream, "\toutput %s ;\n", current.c_str());
    else fprintf(output_stream, "\toutput [%i:0] %s ;\n", width-1, current.c_str());
    _varwidths[current] = width;
  }
  hash_free (repeats);

  //the hidden logic statements
  repeats = hash_new (4);
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
      if (hash_lookup (repeats, current.c_str())) {
	skip = true;
      }
      else {
	hash_add (repeats, current.c_str());
      }
      // omit ports with the same name
      li_name = list_next(li_name);
      if (skip) continue;
      // the bitwidth
      int width = ihash_lookup(outwidthmap, (long) list_value (li))->i;
      if ( width <=0 ) fatal_error("chpexpr2verilog::print_expr_set error: Expression operands have incompatible bit widths\n");
      else if (width == 1 && vectorize==0) fprintf(output_stream, "\twire %s ;\n", current.c_str());
      else fprintf(output_stream, "\twire [%i:0] %s ;\n", width-1, current.c_str());
      list_append (all_names, current.c_str());
      _varwidths[current] = width;
    }
  }
  hash_free (repeats);

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
  repeats = hash_new (4);
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
      if (hash_lookup (repeats, current.c_str())) {
	skip = true;
      }
      else {
	hash_add (repeats, current.c_str());
      }
      li_name = list_next(li_name);
      if (skip) continue;
     // also print the hidden assigns
      int idx = print_expression(output_stream, e, inexprmap);
      auto buf = _gen_dummy_id(idx);
      fprintf(output_stream,"\tassign %s = %s;\n", current.c_str(), buf.c_str());
    }
  }

  //the actuall logic statements
  fprintf(output_stream, "\n\t// the actuall logic statements as assigns\n");
  li_name = list_first (out_expr_name_list);
  for (li = list_first (out_list); li; li = list_next (li))
  {
    Expr *e = (Expr*) list_value (li);
    std::string current = (char *) list_value (li_name);
    int idx = print_expression(output_stream, e, inexprmap);
    auto buf = _gen_dummy_id(idx);
    fprintf(output_stream,"\tassign %s = %s;\n", current.c_str(), buf.c_str());
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
  std::string buf;
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
    buf = _gen_dummy_id(res);					\
    if (resw == 1) {							\
      fprintf (output_stream, "\twire %s;\n", buf.c_str());			\
    }									\
    else {								\
      fprintf (output_stream, "\twire [%d:0] %s;\n", resw-1, buf.c_str());	\
    }									\
    fprintf (output_stream, "\tassign %s = ", buf.c_str());			\
    if (width) {							\
      *width = resw;							\
    }									\
  } while (0)

  lw = -1;
  rw = -1;

  int vw = -1;
  
  switch (e->type) {
  case E_BUILTIN_BOOL:
    lidx = print_expression(output_stream, e->u.e.l, exprmap, &lw);

    /* lhs, res has bitwidth 1 */
    resw = 1;
    DUMP_DECL_ASSIGN;

    /* rhs */
    buf = _gen_dummy_id(lidx);
    fprintf (output_stream, "%s ? 1'b1 : 1'b0", buf.c_str());
    break;

  case E_BUILTIN_INT:
    lidx = print_expression(output_stream, e->u.e.l, exprmap, &lw);
    
    if (!e->u.e.r) {
      resw = 1;
    }
    else {
      resw = e->u.e.r->u.ival.v;
    }
    DUMP_DECL_ASSIGN;

    buf = _gen_dummy_id(lidx);
    fprintf (output_stream, "%s", buf.c_str()); 
    
    break;

  case (E_QUERY):
    tmp = print_expression (output_stream, e->u.e.l, exprmap, &lw);
    lidx = print_expression (output_stream, e->u.e.r->u.e.l, exprmap, &lw);
    ridx = print_expression (output_stream, e->u.e.r->u.e.r, exprmap, &rw);
    resw = act_expr_bitwidth (e->type, lw, rw);

    DUMP_DECL_ASSIGN;
    buf = _gen_dummy_id(tmp);
    fprintf (output_stream, " %s ? ", buf.c_str());
    buf = _gen_dummy_id(lidx);
    fprintf (output_stream, " %s : ", buf.c_str());
    buf = _gen_dummy_id(ridx);
    fprintf (output_stream, " %s", buf.c_str());
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

    buf = _gen_dummy_id(lidx);
    fprintf(output_stream, "%s ", buf.c_str());
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
    buf = _gen_dummy_id(ridx);
    fprintf(output_stream, " %s", buf.c_str());
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
    buf = _gen_dummy_id(lidx);
    fprintf (output_stream, "%s", buf.c_str());
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
    buf = _gen_dummy_id(lidx);
    if (lw < resw) {
      fprintf (output_stream, "{%d'b", resw-lw);
      for (int i=0; i < resw-lw; i++) {
	fprintf (output_stream, "0");
      }
      fprintf (output_stream, ",%s};\n", buf.c_str());
    }
    else if (lw > resw) {
      fprintf (output_stream, "%s[%d:0]", buf.c_str(), resw-1);
    }
    else {
      fprintf (output_stream, "%s;\n", buf.c_str());
    }
    lidx = res;

    DUMP_DECL_ASSIGN;
    buf = _gen_dummy_id(ridx);
    if (rw < resw) {
      fprintf (output_stream, "{%d'b", resw-rw);
      for (int i=0; i < resw-rw; i++) {
	fprintf (output_stream, "0");
      }
      fprintf (output_stream, ",%s};\n", buf.c_str());
    }
    else if (rw > resw) {
      fprintf (output_stream, "%s[%d:0]", buf.c_str(), resw-1);
    }
    else {
      fprintf (output_stream, "%s;\n", buf.c_str());
    }
    ridx = res;
    
    DUMP_DECL_ASSIGN;
    buf = _gen_dummy_id(lidx);
    fprintf(output_stream, "%s ", buf.c_str());
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
    buf = _gen_dummy_id(ridx);
    fprintf (output_stream, " %s", buf.c_str());
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
    buf = _gen_dummy_id(list_ivalue (li));
	  fprintf (output_stream, "%s", buf.c_str());
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
      {
	ihash_bucket_t *b;
	std::string tmp;
	b = ihash_lookup (exprmap, (long)(e));
	Assert (b, "variable not found in variable map");
	tmp = (char *)b->v;
	if (_varwidths.find (tmp) != _varwidths.end()) {
	  vw = _varwidths[tmp];
	  if (l >= _varwidths[tmp]) {
	    l = _varwidths[tmp]-1;
	  }
	}
	else {
	  fatal_error ("Could not find bitwidth for variable %s!\n", (char *)b->v);
	}
      }
      resw = l - r + 1;
      DUMP_DECL_ASSIGN;
      {
	ihash_bucket_t *b;
	b = ihash_lookup (exprmap, (long)(e));
	Assert (b, "variable not found in variable map");
	fprintf(output_stream, "%s", (char *)b->v);
      }
      if (l == vw-1 && r == 0) {
	// do nothing!
      }
      else {
	fprintf(output_stream, " [");
	if (l!=r) {
	  fprintf(output_stream, "%i:", l);
	  fprintf(output_stream, "%i", r);
	} else {
	  fprintf(output_stream, "%i", r);
	}
	fprintf(output_stream, "]");
      }
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


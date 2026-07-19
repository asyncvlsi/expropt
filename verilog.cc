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


static void _collect_var_widths (std::unordered_map<std::string, int> *w,
				 Expr *e,
				 struct iHashtable *leafmap,
				 struct pHashtable *H)
{
  ihash_bucket_t *b;
  phash_bucket_t *pb;
  if (!e) return;

  switch (e->type) {
  case E_INT:
  case E_TRUE:
  case E_FALSE:
  case E_REAL:
    break;

  case E_PROBE:
    break;

  case E_VAR:
    if (!phash_lookup (H, e)) {
      std::string tmp;
      b = ihash_lookup (leafmap, (long)e);
      Assert (b, "Variable not found in varmap");
      tmp = (char *)b->v;
      if (w->find (tmp) != w->end()) {
	pb = phash_add (H, e);
	pb->i = (*w)[tmp];
      }
      else {
	fatal_error ("Could not find bitwidth for variable %s!", (char *)b->v);
      }
    }
    break;

  case E_BITFIELD:
    if (!phash_lookup (H, e->u.e.l)) {
      std::string tmp;
      b = ihash_lookup (leafmap, (long)e);
      Assert (b, "Variable not found in varmap");
      tmp = (char *)b->v;
      if (w->find (tmp) != w->end()) {
	pb = phash_add (H, e->u.e.l);
	pb->i = (*w)[tmp];
      }
      else {
	fatal_error ("Could not find bitwidth for variable %s!", (char *)b->v);
      }
    }
    break;

  default:
    _collect_var_widths (w, e->u.e.l, leafmap, H);
    _collect_var_widths (w, e->u.e.r, leafmap, H);
    break;
  }
  return;
}

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

  std::unordered_map<std::string, int> _varwidths;

  struct pHashtable *_Hexpr;
  struct pHashtable *_Hwidth;

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
      if (!first) fprintf(output_stream, ", ");
      fprintf(output_stream, "%s", current.c_str());
      list_append (all_names, current.c_str());
      first = false;
    }
  }
  hash_free (repeats);

  // ... then outputs
  listitem_t *li_name = list_first (out_expr_name_list);
  repeats = hash_new (4);
  for (li = list_first (out_list); li; li = list_next (li)) {
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
      if (!first) fprintf(output_stream, ", ");
      fprintf(output_stream, "%s", current.c_str());
      list_append (all_names, current.c_str());
      first = false;
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

      /* now walk through the expression, and save the variable widths */
      _collect_var_widths (&_varwidths, e, inexprmap, _Hwidth);
      int dummy_w;
      int idx = _printExpr (output_stream, e, NULL, _dummy_prefix, &_dummy_idx,
			    _Hexpr, _Hwidth, inexprmap, &dummy_w);
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

    /* now walk through the expression, and save the variable widths */
    _collect_var_widths (&_varwidths, e, inexprmap, _Hwidth);
    int dummy_w;
    int idx = _printExpr (output_stream, e, NULL, _dummy_prefix, &_dummy_idx,
			  _Hexpr, _Hwidth, inexprmap, &dummy_w);
    auto buf = _gen_dummy_id(idx);
    fprintf(output_stream,"\tassign %s = %s;\n", current.c_str(), buf.c_str());
    li_name = list_next(li_name);
  }
  fprintf(output_stream, "\nendmodule\n");

  phash_free (_Hexpr);
  phash_free (_Hwidth);
}

static void _collect_vwidths (Scope *sc,
			      struct pHashtable *H,
			      struct pHashtable *tmp,
			      Expr *e)
{
  phash_bucket_t *pb;
  if (!sc || !e) return;

  /* in case we have edags */
  if (phash_lookup (tmp, e)) return;
  pb = phash_add (tmp, e);

  switch (e->type) {
  case E_INT:
  case E_TRUE:
  case E_FALSE:
  case E_REAL:
  case E_PROBE:
    break;

  case E_VAR:
    if (!phash_lookup (H, e)) {
      ActId *id = (ActId *) e->u.e.l;
      InstType *it = sc->FullLookup (id, NULL);
      Assert (it, "ID not found in scope?!");
      pb = phash_add (H, e);
      pb->i = TypeFactory::totBitWidth (it);

      if (id->isDynamicDeref()) {
	// XXX: multi-dimensional arrays?!
	_collect_vwidths (sc, H, tmp, id->arrayInfo()->getDeref(0));
      }
    }
    break;

  case E_BITFIELD:
    if (!phash_lookup (H, e->u.e.l)) {
      ActId *id = (ActId *) e->u.e.l;
      InstType *it = sc->FullLookup (id, NULL);
      Assert (it, "ID not found in scope?!");
      pb = phash_add (H, e);
      pb->i = TypeFactory::totBitWidth (it);
    }
    break;

  default:
    _collect_vwidths (sc, H, tmp, e->u.e.l);
    _collect_vwidths (sc, H, tmp, e->u.e.r);
    break;
  }
}


int ExternalExprOpt::printExpr (FILE *fp, Expr *e, Scope *sc,
				const char *prefix, int *idx,
				iHashtable *leafmap)
{
  int ret;
  int w;
  struct pHashtable *emap, *wmap;

  emap = phash_new (4);
  wmap = phash_new (4);

  _collect_vwidths (sc, wmap, emap, e);
  phash_clear (emap);

  ret = _printExpr (fp, e, sc, prefix, idx, emap, wmap, leafmap, &w);

  phash_free (emap);
  phash_free (wmap);

  return ret;
}

int ExternalExprOpt::_printExpr (FILE *fp, Expr *e, Scope *sc,
				 const char *prefix, int *idx,
				 pHashtable *emap,
				 pHashtable *wmap,
				 iHashtable *leafmap,
				 int *width)
{
  int tmp;
  int lw, rw;
  int lidx, ridx;
  int res, resw;
  std::string buf;
  Expr *orig_e = e;

  phash_bucket_t *b;

  b = phash_lookup (emap, e);
  if (b) {
    phash_bucket_t *b2;
    // we need to set the bitwidth!
    if (width) {
      b2 = phash_lookup (wmap, e);
      *width = b2->i;
    }
    return b->i;
  }

  auto gen_fresh_idx = [&] () -> int {
    int ret;
    if (sc) {
      sc->findFresh (prefix, idx);
      ret = *idx;
    }
    else {
      ret = *idx;
      *idx = *idx + 1;
    }
    return ret;
  };

  auto gen_dummy_id = [&] (int id) -> std::string {
    std::string ret = prefix;
    ret.append (std::to_string (id));
    return ret;
  };

#define DUMP_DECL_ASSIGN						\
  do {									\
    res = gen_fresh_idx ();						\
    buf = gen_dummy_id(res);					\
    if (resw == 1 || resw==0) {							\
      fprintf (fp, "\twire %s;\n", buf.c_str());			\
    }									\
    else {								\
      fprintf (fp, "\twire [%d:0] %s;\n", resw-1, buf.c_str());	\
    }									\
    fprintf (fp, "\tassign %s = ", buf.c_str());			\
    if (width) {							\
      *width = resw;							\
    }									\
  } while (0)

  lw = -1;
  rw = -1;

  int vw = -1;

  switch (e->type) {
  case E_BUILTIN_BOOL:
    lidx = _printExpr (fp, e->u.e.l, sc, prefix, idx,
		       emap, wmap, leafmap, width);
    /* lhs, res has bitwidth 1 */
    resw = 1;
    DUMP_DECL_ASSIGN;

    /* rhs */
    buf = gen_dummy_id(lidx);
    fprintf (fp, "%s != 0", buf.c_str());
    break;

  case E_BUILTIN_INT:
    if (!e->u.e.r) {
      resw = 1;
    }
    else {
      resw = e->u.e.r->u.ival.v;
    }
    if (resw==0) {
      DUMP_DECL_ASSIGN;
      fprintf (fp, "0");
    }
    else {
      lidx = _printExpr (fp, e->u.e.l, sc, prefix, idx,
			 emap, wmap, leafmap, &lw);
      DUMP_DECL_ASSIGN;
      buf = gen_dummy_id(lidx);
      fprintf (fp, "%s", buf.c_str());
    }
    break;

  case (E_QUERY):
    tmp = _printExpr (fp, e->u.e.l, sc, prefix, idx,
		      emap, wmap, leafmap, &lw);
    lidx = _printExpr (fp, e->u.e.r->u.e.l, sc, prefix, idx,
		       emap, wmap, leafmap, &lw);
    ridx = _printExpr (fp, e->u.e.r->u.e.r, sc, prefix, idx,
		       emap, wmap, leafmap, &rw);
    resw = act_expr_bitwidth (e->type, lw, rw);

    DUMP_DECL_ASSIGN;
    buf = gen_dummy_id(tmp);
    fprintf (fp, " %s ? ", buf.c_str());
    buf = gen_dummy_id(lidx);
    fprintf (fp, " %s : ", buf.c_str());
    buf = gen_dummy_id(ridx);
    fprintf (fp, " %s", buf.c_str());
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
    lidx = _printExpr (fp, e->u.e.l, sc, prefix, idx,
		       emap, wmap, leafmap, &lw);
    ridx = _printExpr (fp, e->u.e.r, sc, prefix, idx,
		       emap, wmap, leafmap, &rw);

    resw = act_expr_bitwidth (e->type, lw, rw);
    DUMP_DECL_ASSIGN;

    buf = gen_dummy_id(lidx);
    fprintf(fp, "%s ", buf.c_str());
    if (e->type == E_AND) {
      fprintf (fp, "&");
    }
    else if (e->type == E_OR) {
      fprintf (fp, "|");
    }
    else if (e->type == E_XOR) {
      fprintf (fp, "^");
    }
    else if (e->type == E_DIV) {
      fprintf (fp, "/");
    }
    else if (e->type == E_MOD) {
      fprintf (fp, "%%");
    }
    else if (e->type == E_LSR) {
      fprintf (fp, ">>");
    }
    else if (e->type == E_ASR) {
      fprintf (fp, ">>>");
    }
    else if (e->type == E_LT) {
      fprintf (fp, "<");
    }
    else if (e->type == E_LT) {
      fprintf (fp, "<");
    }
    else if (e->type == E_GT) {
      fprintf (fp, ">");
    }
    else if (e->type == E_LE) {
      fprintf (fp, "<=");
    }
    else if (e->type == E_GE) {
      fprintf (fp, ">=");
    }
    else if (e->type == E_EQ) {
      fprintf (fp, "==");
    }
    else if (e->type == E_NE) {
      fprintf (fp, "!=");
    }
    buf = gen_dummy_id(ridx);
    fprintf(fp, " %s", buf.c_str());
    break;

    /* unary */
  case (E_NOT):
  case (E_COMPLEMENT):
  case E_UMINUS:
    lidx = _printExpr (fp, e->u.e.l, sc, prefix, idx,
		       emap, wmap, leafmap, &lw);
    rw = 0;
    resw = act_expr_bitwidth (e->type, lw, rw);
    DUMP_DECL_ASSIGN;

    if (e->type == E_NOT || e->type == E_COMPLEMENT) {
      fprintf(fp, "~");
    }
    else if (e->type == E_UMINUS) {
      fprintf(fp, "-");
    }
    buf = gen_dummy_id(lidx);
    fprintf (fp, "%s", buf.c_str());
    break;

    /* padding needed */
  case (E_PLUS):
  case (E_MINUS):
  case (E_MULT):
  case (E_LSL):
    lidx = _printExpr (fp, e->u.e.l, sc, prefix, idx,
		       emap, wmap, leafmap, &lw);
    ridx = _printExpr (fp, e->u.e.r, sc, prefix, idx,
		       emap, wmap, leafmap, &rw);
    resw = act_expr_bitwidth (e->type, lw, rw);

    /* pad left */
    DUMP_DECL_ASSIGN;
    buf = gen_dummy_id(lidx);
    if (lw < resw) {
      fprintf (fp, "{%d'b", resw-lw);
      for (int i=0; i < resw-lw; i++) {
	fprintf (fp, "0");
      }
      fprintf (fp, ",%s};\n", buf.c_str());
    }
    else if (lw > resw) {
      fprintf (fp, "%s[%d:0]", buf.c_str(), resw-1);
    }
    else {
      fprintf (fp, "%s;\n", buf.c_str());
    }
    lidx = res;

    DUMP_DECL_ASSIGN;
    buf = gen_dummy_id(ridx);
    if (rw < resw) {
      fprintf (fp, "{%d'b", resw-rw);
      for (int i=0; i < resw-rw; i++) {
	fprintf (fp, "0");
      }
      fprintf (fp, ",%s};\n", buf.c_str());
    }
    else if (rw > resw) {
      fprintf (fp, "%s[%d:0]", buf.c_str(), resw-1);
    }
    else {
      fprintf (fp, "%s;\n", buf.c_str());
    }
    ridx = res;

    DUMP_DECL_ASSIGN;
    buf = gen_dummy_id(lidx);
    fprintf(fp, "%s ", buf.c_str());
    if (e->type == E_PLUS) {
      fprintf (fp, "+");
    }
    else if (e->type == E_MINUS) {
      fprintf (fp, "-");
    }
    else if (e->type == E_MULT) {
      fprintf (fp, "*");
    }
    else if (e->type == E_LSL) {
      fprintf (fp, "<<");
    }
    buf = gen_dummy_id(ridx);
    fprintf (fp, " %s", buf.c_str());
    break;

    case (E_INT):
    {
      ihash_bucket_t *b;
      if (leafmap) {
	b = ihash_lookup (leafmap, (long)(e));
      }
      else {
	b = NULL;
      }
      if (b) {
	resw = 64;
	DUMP_DECL_ASSIGN;
	fprintf(fp, "%s", (char *)b->v);
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
	if (resw==0) {
	  fprintf (fp, "1'b0");
	}
	else {
	  if (bi) {
	    fprintf (fp, "%d'b", resw);
	    bi->bitPrint (fp);
	  }
	  else {
	    fprintf(fp, "%d'h%lx", resw, e->u.ival.v);
	  }
	}
      }
    }
    break;

  case (E_VAR):
    {
      ihash_bucket_t *b;
      phash_bucket_t *pb;

      if (leafmap) {
	b = ihash_lookup (leafmap, (long)(e));
	Assert (b, "variable not found in variable map");
      }
      else {
	b = NULL;
      }
      pb = phash_lookup (wmap, e);
      if (pb) {
	resw = pb->i;
	ActId *tmpid = (ActId *)e->u.e.l;
	if (!b && tmpid->isDynamicDeref()) {
	  int index_w;
	  int index_id =
	    _printExpr (fp, tmpid->arrayInfo()->getDeref (0),
			sc, prefix, idx, emap, wmap, leafmap, &index_w);
	  DUMP_DECL_ASSIGN;

	  /* strip out array and print it separately */
	  Array *ta = tmpid->arrayInfo();
	  tmpid->setArray (NULL);
	  fprintf (fp, "\\");
	  tmpid->Print (fp);
	  fprintf (fp, " ");
	  tmpid->setArray (ta);

	  buf = gen_dummy_id (index_id);
	  fprintf (fp, "[%s]", buf.c_str());
	}
	else {
	  DUMP_DECL_ASSIGN;
	  if (b) {
	    fprintf (fp, "%s", (char*)b->v);
	  }
	  else {
	    // it's actually a simple ID!
	    fprintf (fp,  "\\");
	    tmpid->Print (fp);
	    fprintf (fp, " ");
	  }
	}
      }
      else {
	fatal_error ("Could not find bitwidth for variable %s!\n",
		     b ? (char *)b->v : "??");
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
      b = ihash_lookup (leafmap, (long)(e));
      if (b) fprintf(fp, "%s", (char *)b->v);
      else fprintf(fp, " 1'b1 ");
    }
    break;
    case (E_FALSE):
    {
      ihash_bucket_t *b;
      resw = 1;
      DUMP_DECL_ASSIGN;
      b = ihash_lookup (leafmap, (long)(e));
      if (b) fprintf(fp, "%s", (char *)b->v);
      else fprintf(fp, " 1'b0 ");
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
	  lidx = _printExpr (fp, e->u.e.l, sc, prefix, idx,
			     emap, wmap, leafmap, &lw);
	  if (lw>0) {
	    resw += lw;
	    list_iappend (resl, lidx);
	  }
	  e = e->u.e.r;
	}
	DUMP_DECL_ASSIGN;
	if (list_isempty(resl)) {
	  fprintf (fp, "0");
	}
	else {
	  fprintf (fp, "{");
	  for (listitem_t *li = list_first (resl); li; li = list_next (li)) {
	    buf = gen_dummy_id(list_ivalue (li));
	    fprintf (fp, "%s", buf.c_str());
	    if (list_next (li)) {
	      fprintf (fp, ", ");
	    }
	  }
	}
	fprintf (fp, "}");
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
	phash_bucket_t *pb;
	std::string tmp;
	b = ihash_lookup (leafmap, (long)(e));
	Assert (b, "variable not found in variable map");
	tmp = (char *)b->v;
	pb = phash_lookup (wmap, e->u.e.l);
	if (pb) {
	  vw = pb->i;
	  if (l >= pb->i) {
	    l = pb->i - 1;
	  }
	}
	else {
	  fatal_error ("Could not find bitwidth for variable %s!\n", tmp.c_str());
	}
      }
      resw = l - r + 1;
      DUMP_DECL_ASSIGN;
      {
	ihash_bucket_t *b;
	b = ihash_lookup (leafmap, (long)(e));
	Assert (b, "variable not found in variable map");
	fprintf(fp, "%s", (char *)b->v);
      }
      if (l == vw-1 && r == 0) {
	// do nothing!
      }
      else {
	fprintf(fp, " [");
	if (l!=r) {
	  fprintf(fp, "%i:", l);
	  fprintf(fp, "%i", r);
	} else {
	  fprintf(fp, "%i", r);
	}
	fprintf(fp, "]");
      }
      break;

    case (E_REAL):
      fatal_error ("No reals!");
      break;
    case (E_RAWFREE):
      fprintf(fp, "RAWFREE\n");
      fatal_error("%u should have been handled else where", e->type);
      break;
    case (E_END):
      fprintf(fp, "END\n");
      fatal_error("%u should have been handled else where", e->type);
      break;
    case (E_NUMBER):
      fprintf(fp, "NUMBER\n");
      fatal_error("%u should have been handled else where", e->type);
      break;
    case (E_FUNCTION):
      fprintf(fp, "FUNCTION\n");
      fatal_error("%u should have been handled else where", e->type);
      break;
    default:
      fprintf(fp, "Whaaat?! %i\n", e->type);
      break;
  }
  fprintf (fp, ";\n");
  b = phash_add (emap, orig_e);
  b->i = res;
  if (width && orig_e->type != E_VAR) {
    b = phash_add (wmap, orig_e);
    b->i = *width;
  }
  return res;
}

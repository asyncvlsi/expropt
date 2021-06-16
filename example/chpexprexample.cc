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
 **************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <ctype.h>
#include <stdbool.h>
#include <act/types.h>
#include "chpexprexample.h"

/*
* create, prepare and copy all the data structurs and instancialte the EXPROPT lib.
*/
chpexprexample::chpexprexample(char *out, expr_mapping_software software, Act *act_instance, Process *process_instance)
{
  optimiser = new ExternalExprOpt(genus,bd,false);
  output_file_path = out;
  p = process_instance;
  a = act_instance;
  out_expr_bundle = list_new();
  expr_set_number = 0;
  mapper = software;
  inexprmap = ihash_new(0);
  inwidthmap = ihash_new(0);
  outexprmap = ihash_new(0);
  outwidthmap = ihash_new(0);
}

/*
* clean the global lists and maps after beeing done, they should be all empty anyway
*/
chpexprexample::~chpexprexample()
{
  list_free(out_expr_bundle);
  ihash_free(inexprmap);
  ihash_free(inwidthmap);
  ihash_free(outexprmap);
  ihash_free(outwidthmap);
}

/* 
 * the main method here we flush out our recursivly collected expressione to the EXPROPT lib
 */
void chpexprexample::start_new_set ()
{
  if (list_isempty(out_expr_bundle))
  {
    return;
  }
  char expr_set_name[1024] = "";
  sprintf(expr_set_name, "set%u", expr_set_number);
  printf("result: %s %p\n", expr_set_name, expr_set_name);
  list_t* in_expr_bundle = list_new();
  listitem_t *li;
  // generate the list of input leafs
  for (li = list_first (out_expr_bundle); li; li = list_next (li))
  {
    search_var_in_expr (in_expr_bundle, (Expr *) list_value(li));
  }
  ExprBlockInfo* info;
  // run the syntesis
  info = optimiser->run_external_opt(expr_set_name, in_expr_bundle, inexprmap, inwidthmap, out_expr_bundle, outexprmap, outwidthmap);

  printf("Generated block %s: Area: %e m2, Power: %e W, delay: %e s, max power: %e, min delay: %e, max delay: %e, static power: %e, dynamic power: %e (if 0 => circuit empty, extraction failed or corner not provided)\n", 
      expr_set_name, info->area, info->power_typ, info->delay_typ, info->power_max, info->delay_min, info->delay_max, info->power_typ_static, info->power_typ_dynamic);

  // keep track and clean up, create freh set
  expr_set_number++;
  list_free(out_expr_bundle);
  list_free(in_expr_bundle);
  out_expr_bundle = list_new();
}

/*
 * walk recusivly through the expresion and add all leafs E_VAR the the in list
 */
void chpexprexample::search_var_in_expr (list_t * in_bundle, Expr *e)
{
  switch (e->type) {
    case (E_PROBE):
			warning("probes are not working yet");
    case (E_VAR):
    {
      list_append(in_bundle, e);
      char *charbuf = (char*)malloc( sizeof(char) * ( 1024 + 1 ) );
      ihash_bucket_t *b_expr, *b_width;
      b_expr = ihash_add(inexprmap,(long) e);
      ((ActId*)(e->u.e.l))->sPrint(charbuf, 1024);
      printf("added in %s\n",charbuf);
      b_expr->v = charbuf;
      b_width = ihash_add(inwidthmap,(long) e);
      b_width->i = TypeFactory::bitWidth(p->Lookup((ActId*)e->u.e.l));
    }
      break;
    case (E_AND):
    case (E_OR):
    case (E_PLUS):
    case (E_MINUS):
    case (E_MULT):
    case (E_DIV):
    case (E_MOD):
    case (E_LSL):
    case (E_LSR):
    case (E_ASR):
    case (E_XOR):
    case (E_LT):
    case (E_GT):
    case (E_LE):
    case (E_GE):
    case (E_EQ):
    case (E_NE):
      search_var_in_expr(in_bundle, e->u.e.l);
      search_var_in_expr(in_bundle, e->u.e.r);
      break;
    case (E_NOT):
    case (E_UMINUS):
    case (E_COMPLEMENT):
      search_var_in_expr(in_bundle, e->u.e.l);
      break;

    case (E_QUERY):
			search_var_in_expr(in_bundle, e->u.e.l);
			search_var_in_expr(in_bundle, e->u.e.r->u.e.l);
			search_var_in_expr(in_bundle, e->u.e.r->u.e.r);
      break;

    case (E_CONCAT):
      search_var_in_expr(in_bundle, e->u.e.l);
			if (e->u.e.r) {
  	    search_var_in_expr(in_bundle, e->u.e.r);
			}
      break;
    case (E_INT):
    case (E_BITFIELD):
    case (E_LPAR):
    case (E_RPAR):
    case (E_TRUE):
    case (E_FALSE):
    case (E_COLON):
    case (E_COMMA):
    case (E_REAL):
    case (E_RAWFREE):
    case (E_END):
    case (E_NUMBER):
    case (E_FUNCTION):
      break;
    default:
      fatal_error( "Whaaat?! %i\n", e->type);
      break;
  }
}

/*
 * go though the chp data structure and add new expressions to the sets, 
 * also add the names they map to and the width in the maps. flush the set out if a sqecuential op is encountered
 */
void chpexprexample::search_expr (act_chp_lang_t *c)
{
  int expr_width;
  InstType *it;

  switch (c->type)
  {
    // get the expr form the assign
    case ACT_CHP_ASSIGN:
    {
      char *charbuf = (char*)malloc( sizeof(char) * ( 1024 + 1 ) );
      ihash_bucket_t *b_expr, *b_width;
      b_expr = ihash_add(outexprmap,(long) c->u.assign.e);
      c->u.assign.id->sPrint(charbuf, 1024);
      printf("added assign %s\n",charbuf);
      b_expr->v = charbuf;
      b_width = ihash_add(outwidthmap,(long) c->u.assign.e);
      b_width->i = TypeFactory::bitWidth(p->Lookup(c->u.assign.id));
      list_append(out_expr_bundle,c->u.assign.e);
    }
      break;

    // get the expression from the send
    case ACT_CHP_SEND:
    {
      // for send first itmen is expression second is var/actid
      char *charbuf = (char*)malloc( sizeof(char) * ( 1024 + 1 ) );
      ihash_bucket_t *b_expr, *b_width;
      b_expr = ihash_add(outexprmap,(long) c->u.comm.e);
      c->u.comm.chan->sPrint(charbuf, 1024);
      printf("added send %s\n",charbuf);
      b_expr->v = charbuf;
      b_width = ihash_add(outwidthmap,(long) c->u.comm.e);
      b_width->i = TypeFactory::bitWidth(p->Lookup(c->u.comm.chan));
      list_append(out_expr_bundle,c->u.comm.e);
    }
      break;

    // comma is paralell thats perfect continure
    case ACT_CHP_COMMA:
    {
      listitem_t *li;
      for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li))
      {
        act_chp_lang_t *cmd = (act_chp_lang_t *) list_value (li);
        search_expr (cmd);
      }
    }
      break;
    // loops and selects can have all guards bundeled but need sequential exec for the satements so they have to go in seperate blocks
    case ACT_CHP_SELECT:
    case ACT_CHP_LOOP:
    {
      // // all guards at the same time
      start_new_set();
      act_chp_gc_t *gc = c->u.gc;
      while (gc)
      {
        if (gc->g == NULL){
          char *charbuf = (char*)malloc( sizeof(char) * ( 1024 + 1 ) );
          ihash_bucket_t *b_expr, *b_width;
          Expr* tmpexpr = new Expr();
          tmpexpr->type = E_TRUE;
          tmpexpr->u.v = 1;
          b_expr = ihash_add(outexprmap,(long) tmpexpr);
          a->msnprintf(charbuf,1024,"guardTRUE");
          printf("added %p guard %s\n",charbuf,charbuf);
          b_expr->v = charbuf;
          b_width = ihash_add(outwidthmap,(long) tmpexpr);
          b_width->i = 1;
          list_append(out_expr_bundle, tmpexpr);
          gc = gc->next;
        }
        else
        {
          char *charbuf = (char*)malloc( sizeof(char) * ( 1024 + 1 ) );
          ihash_bucket_t *b_expr, *b_width;
          b_expr = ihash_add(outexprmap,(long) gc->g);
          a->msnprintf(charbuf,1024,"guard%p",gc->g);
          printf("added guard %s\n",charbuf);
          b_expr->v = charbuf;
          b_width = ihash_add(outwidthmap,(long) gc->g);
          b_width->i = 1;
          list_append(out_expr_bundle, gc->g);
          gc = gc->next;
        }
        
      }
      
      gc = c->u.gc;

      while (gc)
      {
        //statements only
        start_new_set();
        search_expr(gc->s);
        
        gc = gc->next;
      }
    }
      break;
    // bad luck semil colon is sequential, start a new set
    case ACT_CHP_SEMI:
    {
      listitem_t *li;
      for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li))
      {
        start_new_set();
        act_chp_lang_t *cmd = (act_chp_lang_t *) list_value (li);
        search_expr (cmd);
      }
    }
      break;
    case ACT_CHP_RECV:
    default:
      start_new_set();
      break;
  }  
  return;
}

// go and process the chp
void chpexprexample::process_chp()
{
  search_expr(p->getlang()->getchp()->c);
  start_new_set();
}


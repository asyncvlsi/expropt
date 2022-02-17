/*************************************************************************
 *
 *  This file is part of expropt
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
#ifndef __chpexprexample_H__
#define __chpexprexample_H__

#include "../src/expropt.h"
#include <act/lang/act.h>
#include <string>

/**
 * this is an example class based on chpcheck from chp2prs,
 * that will extract all expression statements from chp and group them and than send them to the optimisation tool.
 *
 * the expressions are collected in global list, and everytime we encounter a chp structure that is sequential we
 * flush the collected list out the the optimisation tool.
 */
class chpexprexample {
  public:
    /**
     * Construct a new chpexprexample object, and pass the process over.
     *
     * @param out the file where all the optiomised block will be appended to.
     * @param software which software to use.
     * @param act_instance the global act instance for tooling.
     * @param process_instance the process that it should extract the expressions out.
     */
    chpexprexample(char *out, expr_mapping_software software, Act *act_instance, Process *process_instance);
    /**
     * Destroy the chpexprexample object.
     */
    ~chpexprexample();
    /**
     * run the pass on the act chp process structure
     *
     * @return * run
     */
    void process_chp();

    /**
     * the instance that does all the work
     */
    ExternalExprOpt *optimiser;

  private:
    /**
     * the chp act process to be woked on
     */
    Process *p;

    /**
     * act instance e.g. for mangeling
     */
    Act *a;

    /**
     * the list in which we collect the expr data structures.
     */
    list_t *out_expr_bundle;

    /**
     * the map that maps the expr pointers as long int to the names that are printed in the optimised act, leaf variabes
     * only
     */
    iHashtable *inexprmap;

    /**
     * the map that maps the expr pointers as long int to the bit width of the leaf variables
     */
    iHashtable *inwidthmap;

    /**
     * the map that maps the expr pointers as long int to the names that are printed in the optimised act, output
     * expr/ports only
     */
    iHashtable *outexprmap;

    /**
     * the map that maps the expr pointers as long int to the bit width of the output expr
     */
    iHashtable *outwidthmap;

    /**
     * the software to be used
     */
    expr_mapping_software mapper;

    /**
     * to keep track of the IDs, so they are unique just increment
     */
    int expr_set_number;

    /**
     * the file for the expression block to be appended to
     */
    char *output_file_path;

    /**
     * in a given expression collect all leaf variables
     *
     * @param var_list the list the variables should be added to
     * @param e the expresion to be recursivly searched.
     */
    void search_var_in_expr(list_t *var_list, const Expr *e);

    /**
     * go through the chp data structure and grab all expressions and guards
     *
     * @param c the chp data structure
     */
    void search_expr(const act_chp_lang_t *c);

    /**
     * if you encounter a sequential operation in the chp data structure use this mehtod to flush out a set op
     * expression that are paralell. it will also call the EXPROPT lib for running the syntesis
     */
    void start_new_set();
};

#endif

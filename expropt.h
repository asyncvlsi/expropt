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
#ifndef __EXPR_OPT_H__
#define __EXPR_OPT_H__

#include <string>
#include <regex>
#include <fstream>
#include <unordered_map>
#include "expr_info.h"

/**
 * ExternalExprOpt is an interface that wrapps the syntesis, optimisation and mapping to cells of a set of act expr.
 * it will also give you metadata back if the software supports it. 
 * if you 
 */
class ExternalExprOpt
{
public:
  /**
   * Construct a new ExternalExpOpt generator, supply it with all
   * the settings needed.
   *
   * Most of the technology settings are loaded via the
   * configuration file expropt.conf
   * 
   * @param datapath_syntesis_tool which tool to use; NULL = built-in abc
   *
   * @param mapping_target are we mapping for bundled data or qdi
   *
   * @param expr_file_path the output file path, this is the file
   * all the optimised expression blocks will be saved to. if empty
   * only metadata will be extracted.
   *
   * @param exprid_prefix optional - the prefix for all in and
   * outputs  - if integer id mode is used
   *
   * @param block_prefix the prefix for the expression block - if
   * integer id mode is used
   */
  ExternalExprOpt( const char *datapath_syntesis_tool,
		   const expr_mapping_target mapping_target,
		   const bool tie_cells,
		   const std::string expr_file_path = "",
		   const std::string exprid_prefix = "e",
		   const std::string block_prefix = "blk") 
                    :
                        expr_output_file(expr_file_path),
			expr_prefix(exprid_prefix),
			module_prefix(block_prefix),
			mapper(datapath_syntesis_tool),
			use_tie_cells(tie_cells),
                        wire_encoding(mapping_target)
  {
    _init_defaults ();

    // The difference between QDI and BD is just that the cells are
    // different.
    if (wire_encoding == qdi) {
      cell_act_file = config_get_string("synth.qdi.cell_lib");
      cell_namespace = config_get_string("synth.qdi.cell_lib_namespace");
      expr_channel_type = config_get_string("synth.qdi.cell_lib_wire_type");
    }
    else {
      cell_act_file = config_get_string("synth.bundled.cell_lib");
      cell_namespace = config_get_string("synth.bundled.cell_lib_namespace");
      expr_channel_type = config_get_string("synth.bundled.cell_lib_wire_type");
    }

    cell_namespace_save = cell_namespace;
    
    _cleanup = config_get_int("synth.expropt.clean_tmp_files");

    _abc_api = NULL;
  }

  // cleanup and delete abc if it was started
  ~ExternalExprOpt();


  // return true if the synthesis engine specified exists
  static bool engineExists (const char *name);

  /**
   *  The common steps for the external expression optimizer are:
   *
   *    (a) Convert the ACT expression(s) into their equivalent
   *    Verilog, respecting the different bit-width conventions of the
   *    two languages. As the library is written as a standalone
   *    component, bit-width information is passed into this library
   *    through hash tables.
   *
   *    (b) Call a logic optimization tool on the Verilog design. This
   *    can be abc (built-in), or any other external optimization tool
   *    like yosys, Cadence genus, or Synopsys design compiler.
   *
   *    (c) Translate the synthesized and tech mapped design back into
   *    an ACT design. This uses the v2act tool distributed in the core
   *    ACT library.
   *
   *    (d) Append the generated ACT to a file
   *
   *  There are a number of ways to call the expression optimizer and
   *  control the generated ACT. This impacts the process name used as
   *  well as the names of the ports in the generated processes.
   *
   *
   * 1. INTEGER ID MODE : used to optimize a single expression.
   * (This is the mode used by chp2prs.)
   * 
   * The port list parameters are specified using an integer ID
   * (provided by in_expr_map), and the prefix for the port is given by
   * exprid_prefix (constructor).
   * 
   * The process name uses the block_prefix (constructor), and the
   * integer suffix used is specified by the run_external_opt() call.
   *
   * 2. C-STRING MODE : the process name to be used is provided. Also,
   * the names of the input ports and output ports are also
   * provided. Hidden variables (internal shared nodes) can be
   * specified as well.
   *
   */
  

  /**
   * INTEGER ID MODE - single expr run_external_opt will take one
   * expression "e" and synthesize, optimise and map it to a gate
   * library.
   * 
   * The resulting block will be appended to the output file, the
   * block will be named with the block prefix and the given id
   * "expr_set_number", the output will use "out". All the inputs
   * are named with expr_prefix and there respective id given in the
   * "in_expr_map".  the width of the variables will be specified as
   * the even elements in the input list, so first the ID and than
   * the width.  the output is using "out" and the width defined in
   * target width.
   *
   * The implementation of this function converts the simplified
   * arguments into a call to the simple C-STRING MODE version.
   * 
   * @param expr_set_number the number for the block and the output.
   * @param targetwidth number of the output width/number of wires
   * @param e the expresion to be optimised and mapped
   * @param in_expr_list an act list of expr leaf data
   * structures/variables, they should be E_VAR and for bundled data
   * additional E_INT, E_TRUE, E_FALSE, ...
   * @param in_expr_map the map from pointer (as long int) of the
   * expr struct to char* strings, if a mapping for eg. E_INT is
   * defind it will take precidence over printing the value, E_VAR
   * has to have a mapping.
   * @param in_width_map the map from pointer (as long int) of the
   * expr struct to int for how many wires the expression is
   * referencing to, so the width of the specifig input bus.
   */
  ExprBlockInfo* run_external_opt (int expr_set_number,
				   int targetwidth,
				   Expr *e,
				   list_t *in_expr_list,
				   iHashtable *in_expr_map,
				   iHashtable *in_width_map);

  /**
   * Simple C-STRING MODE - set of expr - recomended mode - outputs are
   * unique.
   *
   * run_external_opt() will take a set of expressions and syntesise,
   * optimise and map it to a gate library.  The resulting process
   * will be appended to the output file, the process will be named with
   * expr_set_name.
   *
   * In and out have to be seperate, because in the IN case you
   * reference the expression variable/port itself and for the
   * outputs the variable/port its assigned to and not itself.  the
   * optional hidden expression list works the same way, it will
   * source the properties for its variables from in maps, and the
   * assign to from the out maps.
   * 
   * @param expr_set_name the name of the verilog module
   *
   * @param in_expr_list an act list of expr leaf data
   * structures/variables, they should be E_VAR
   * @param in_expr_map the map from pointer (as long int) of the expr
   * struct to char* strings, if a mapping for eg. E_INT is defind it
   * will take precidence over printing the value, E_VAR has to have a
   * mapping.
   * @param in_width_map the map from pointer (as long int) of the
   * expr struct to int for how many wires the expression is
   * referencing to, so the width of the specifig input bus.
   * @param out_expr_list an act list for the full expression so the
   * head of the data structure, that are outputs of to be mapped
   * module.
   * @param out_expr_map the map from pointer (as long int) of the
   * expr struct to char* strings, with the name the result of the
   * expression is assinged to.
   * @param out_width_map the map from pointer (as long int) of the
   * expr struct to int, for the bus width of the output
   * @param hidden_expr_list optional - like out_list just the assigns
   * are not used on the outputs, they can be used leafs for the
   * outputs again when using the same char* string name.
   */
  ExprBlockInfo* run_external_opt (const char* expr_set_name,
				   list_t *in_expr_list,
				   iHashtable *in_expr_map,
				   iHashtable *in_width_map,
				   list_t *out_expr_list,
				   iHashtable *out_expr_map,
				   iHashtable *out_width_map,
				   list_t *hidden_expr_list = NULL);

  
  /**
   * General C-STRING MODE - set of expr - with copy on output for non
   * unique outputs/hidden expressions.
   *
   * run_external_opt() will take a set of expressions and syntesise,
   * optimise and map it to a gate library. The resulting block will
   * be appended to the output file, the block will be named with
   * expr_set_name,
   *
   * In and out have to be seperate, because in the IN case you
   * refference the expression variable/port itself and for the
   * outputs the variable/port its assigned to and not itself.
   * the optional hidden expression list works the same way, it will
   * source the properties for its variables from in maps, and the
   * assing to from the out maps.
   * 
   * @param expr_set_name the name of the verilog module
   * @param in_expr_list an act list of expr leaf data
   * structures/variables, they should be E_VAR
   * @param in_expr_map the map from pointer (as long int) of the expr
   * struct to char* strings, if a mapping for eg. E_INT is defind it
   * will take precidence over printing the value, E_VAR has to have a
   * mapping. 
   * @param in_width_map the map from pointer (as long int) of the
   * expr struct to int for how many wires the expression is
   * referencing to, so the width of the specifig input bus. 
   * @param out_expr_list an act list for the full expression so the
   * head of the data structure, that are outputs of to be mapped
   * module.  
   * @param out_expr_name_list an index aligned list (with regards to
   * out_expr_list) with the name (C string pointer) the result of the
   * expression is assinged to. 
   * @param out_width_map the map from pointer (as long int) of the
   * expr struct to int, for the bus width of the output 
   * @param hidden_expr_list optional - like out_list just the assigns
   * are not used on the outputs, they can be used leafs for the
   * outputs again when using the same char* string name. 
   * @param hidden_expr_name_list an index aligned list (with regards
   * to hidden_expr_list) with the name (C string pointer) the result
   * of the expression is assinged to
   */
  ExprBlockInfo* run_external_opt (const char* expr_set_name,
				   list_t *in_expr_list,
				   iHashtable *in_expr_map,
				   iHashtable *in_width_map,
				   list_t *out_expr_list,
				   list_t *out_expr_name_list,
				   iHashtable *out_width_map,
				   list_t *hidden_expr_list = NULL,
				   list_t *hidden_expr_name_list = NULL);


protected:

  void _init_defaults(); 	//< initialize default parameters for
				//< configuration, and read expropt.conf
				//< to override any of the defaults.

  bool _cleanup;
  
  /**
   * print the verilog module, internal takes the inputs and outputs as lists of expressions (plus the properites name and width as maps). 
   * In and out have to be seperate, because in the in case you mean the expression var itself and for the outputs what its assigned to.
     * the optional hidden expression list works the same way, it will source the properties for its variables from in maps, and the assing to from the out maps.
     * 
     * @param output_stream the file its printed to, fatal error if file not open.
     * @param expr_set_name the name of the verilog module
     * @param in_list an act list of expr leaf data structures, they should be E_VAR and for bundled dat additional E_INT, E_TRUE, E_FALSE, ...
     * @param inexprmap the map from pointer of the expr struct to char* strings, if a mapping for eg. E_INT is defind it will take precidence, E_VAR has to have a mapping
     * @param inwidthmap the map for how many wires the expression is referencing to, so the width of the bus.
     * @param out_list an act list for the full expression so the head of the data structure, that are outputs of module. 
     * @param out_name_list an index alligned list containing char* strings, with the name the result of the expression is assinged to.
     * @param outwidthmap the map from pointer of the expr struct to int, for the bus width of the output
     * @param expr_list optional - like out_list just the assigns are not used on the outputs, they can be used leafs for the outputs again when using the same char* string name.
     * @param hidden_name_list optinal - an index alligned list containing char* strings, with the name the result of the expression is assinged to.
     */
    void print_expr_verilog (FILE *output_stream,
			     const char* expr_set_name,
			     list_t *in_list,
			     iHashtable *inexprmap,
			     iHashtable *inwidthmap,
			     list_t *out_list,
			     list_t *out_name_list,
			     iHashtable *outwidthmap,
			     list_t *expr_list = NULL,
			     list_t *hidden_name_list = NULL);
    

    /**
     * the recursive method to print the expression itself as the rhs of a verilog assign.
     * 
     * @param output_stream the file stream to be printed to
     * @param e the expression to be printed to
     * @param exprmap the in_expr_map containing the leaf mappings, E_VAR mappings are required, other leaf mappings are optional, but take precidence over
     * printing the hard coded value. 
     * @return the dummy idx that holds the result
     */
  int print_expression(FILE *output_stream, Expr *e, iHashtable *exprmap,
		       int *width = NULL);



    /**
     * the output file name where all act results are appended too.
     */
    const std::string expr_output_file;

    /**
     * the act cell lib read from the config file
     */
    std::string cell_act_file;

    /**
     * the name space of the cell act file, read from the config file
     * the default is "syn"
     */
    std::string cell_namespace;
    std::string cell_namespace_save;

    void set_namespace(std::string s) { cell_namespace = s; }
    void reset_namespace() { cell_namespace = cell_namespace_save; }

    /**
     * what wire type is used in v2act, if bool is chosen v2act will act in sync mode for all other it runs in async mode.
     * default is "bool" for bd and "dualrail" for qdi
     */
    std::string expr_channel_type;

    /**
     * if used in the integer mode the prefix for each expression in and out.
     * default is "e"
     */
    const std::string expr_prefix;

    /**
     * the module prefeix when used in integer mode.
     * default is "blk"
     */
    const std::string module_prefix;

    /**
     * the software to be used for syntesis and mapping.
     */
    const char *mapper;

    /**
     * should tie cells be incerted by the syntesis software (true), or should v2act take cae of it (false)
     */
    const bool use_tie_cells;

    /**
     * is it a bundeld data or dualrail pass?
     */
    const expr_mapping_target wire_encoding;

    /**
     * used to generate dummy prefix for temp vars
     */
    char _dummy_prefix[10];
    int _dummy_idx;
    void _gen_dummy_id (char *buf, int sz, int idx) {
      snprintf (buf, sz, "%s%d", _dummy_prefix, idx);
    }
    int _gen_fresh_idx () { return _dummy_idx++; }

    struct pHashtable *_Hexpr;
    struct pHashtable *_Hwidth;

    /**
     * This must be an E_VAR
     */
    std::unordered_map<std::string, int> _varwidths;
    int _get_bitwidth (Expr *e);

    /**
     * This is a boxed pointer to the abc API
     */
    void *_abc_api;

    bool (*_syn_run) (act_syn_info *s);
    double (*_syn_get_metric) (act_syn_info *s, expropt_metadata type);
    void (*_syn_cleanup) (act_syn_info *s);
    void *_syn_dlib;

    int _filenum;
};


#endif /* __EXPR_OPT_H__ */

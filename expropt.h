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

#include <act/act.h>
#include <string>
#include <regex>
#include <fstream>
#include <unordered_map>

/**
 * enum for referencing the mapper software type, to define which external syntesis tool to use for syntesis
 */
enum expr_mapping_software {
        yosys = 0,
        synopsis = 1,
        genus = 2
    };

/**
 * enum for referencing the pass type
 */
enum expr_mapping_target {
        qdi = 0,
        bd = 1,
    };

/**
 * the metadata object holds all extracted points of the expr set.
 */
class ExprBlockInfo
{
private:
    /* data */
public:
    /**
     * the typical delay corner as normal temp, in seconds.
     * 0 if not extracted.
     */
    const double delay_typ;

    /**
     * the max delay corner as normal high and slow slow corner, in seconds.
     * 0 if not extracted.
     */
    const double delay_max;

    /**
     * the min delay corner as low temp and fast fast corner, in seconds.
     * 0 if not extracted.
     */
    const double delay_min;

    /**
     * the typical power corner at normal temp, in A?.
     * 0 if not extracted.
     */
    const double power_typ;

    /**
     * the typical power corner at normal temp, in A?.
     * 0 if not extracted.
     */
    const double power_typ_static;

    /**
     * the typical power corner at normal temp, in A?.
     * 0 if not extracted.
     */
    const double power_typ_dynamic;

    /**
     * the high power corner at high temp and fast fast corner, in A?.
     * 0 if not extracted.
     */
    const double power_max;

    /**
     * the typical power corner at normal temp, in A?.
     * 0 if not extracted.
     */
    const double power_max_static;

    /**
     * the typical power corner at normal temp, in A?.
     * 0 if not extracted.
     */
    const double power_max_dynamic;


    /**
     * the theoretical area of all gates combined, with 100% utiliasation.
     * 0 if not extracted.
     */
    const double area;

    /**
     * Construct a new Expr Block Info object, values can not be changed after creation.
     * 
     * parameter see getter descriptions.
     */
    ExprBlockInfo( 
                    const double e_delay_typ,
                    const double e_delay_min,
                    const double e_delay_max,
                    const double e_power_typ,
                    const double e_power_max,
                    const double e_area,
                    const double e_power_typ_static = 0,
                    const double e_power_typ_dynamic = 0,
                    const double e_power_max_static = 0,
                    const double e_power_max_dynamic = 0) : 
                    delay_typ(e_delay_typ),
                    delay_max(e_delay_max),
                    delay_min(e_delay_min),
                    power_typ(e_power_typ),
                    power_typ_static(e_power_typ_static),
                    power_typ_dynamic(e_power_typ_dynamic),
                    power_max(e_power_max),
                    power_max_static(e_power_max_static),
                    power_max_dynamic(e_power_max_dynamic),
                    area(e_area) { }
    
    /**
     * Construct a new Expr Block dummy with no extration results =>
     * all 0, but delay_typ = -1 to indicate that the results were not
     * created.
     */
    ExprBlockInfo( ) : 
                    delay_typ(-1),
                    delay_max(0),
                    delay_min(0),
                    power_typ(0),
                    power_typ_static(0),
                    power_typ_dynamic(0),
                    power_max(0),
                    power_max_static(0),
                    power_max_dynamic(0),
                    area(0){ }
    ~ExprBlockInfo() { }
};

/**
 * ExternalExprOpt is an interface that wrapps the syntesis, optimisation and mapping to cells of a set of act expr.
 * it will also give you metadata back if the software supports it. 
 * if you 
 */
class ExternalExprOpt
{
public:
    /**
     * INTEGER ID MODE - single expr
     * run_external_opt will take one expression "e" and syntesise, optimise and map it to a gate library.
     * 
     * the resulting block will be appended to the output file, the block will be named with the block prefix and the given id "expr_set_number",
     * the output will use "out". All the inputs are named with expr_prefix and there respective id given in the "in_expr_map".
     * the width of the variables will be specified as the even elements in the input list, so first the ID and than the width.
     * the output is using "out" and the width defined in target width.
     * 
     * @param expr_set_number the number for the block and the output.
     * @param targetwidth number of the output width/number of wires
     * @param e the expresion to be optimised and mapped
     * @param in_expr_list an act list of expr leaf data structures/variables, they should be E_VAR and for bundled data additional E_INT, E_TRUE, E_FALSE, ...
     * @param in_expr_map the map from pointer (as long int) of the expr struct to char* strings, if a mapping for eg. E_INT is defind it will take precidence over printing the value, E_VAR has to have a mapping.
     * @param in_width_map the map from pointer (as long int) of the expr struct to int for how many wires the expression is referencing to, so the width of the specifig input bus.
     */
    ExprBlockInfo* run_external_opt (int expr_set_number, int targetwidth, Expr *e, list_t *in_expr_list, iHashtable *in_expr_map, iHashtable *in_width_map);

    /**
     * INTEGER ID MODE - set of expr - do not use
     * run_external_opt will take a list of expressions "expr_list" and syntesise, optimise and map it to a gate library.
     * this list must belong to the same logic step, the optimisation will share gates.
     * 
     * the resulting block will be appended to the output file, the block will be named with the block prefix and the given id "expr_set_number",
     * the outputs will use the expression prefix and the ids from the output list. 
     * All the inputs are named with expr_prefix and there respective id given in the "exprmap"/in_list.
     * the width of the variables will be specified as the even elements in the input and output lists, so first the ID and than the width.
     * 
     * @param expr_set_number  the number for the block, its appended the the block prefix.
     * @param expr_list the set of expressions to be optimised
     * @param in_list the list of input id with the width, first element id, second element width of previous id
     * @param out_list the list of output id with the width, first element id, second element width of previous id, the order must be alinged with the expression list.
     * @param exprmap the pointer of the expresion is the key the id is the value, this is to map the variables inside the expression to the corresponding IDs
     */
    ExprBlockInfo* run_external_opt (int expr_set_number, list_t *expr_list, list_t *in_list, list_t *out_list, iHashtable *exprmap);

    /**
     * C-STRING MODE - set of expr - recomended mode - outputs are unique
     * run_external_opt will take a set of expressions and syntesise, optimise and map it to a gate library.
     * 
     * the resulting block will be appended to the output file, the block will be named with expr_set_name,
     * In and out have to be seperate, because in the IN case you refference the expression variable/port itself and for the outputs the variable/port its assigned to and not itself.
     * the optional hidden expression list works the same way, it will source the properties for its variables from in maps, and the assing to from the out maps.
     * 
     * @param expr_set_name the name of the verilog module
     * @param in_expr_list an act list of expr leaf data structures/variables, they should be E_VAR and for bundled data additional E_INT, E_TRUE, E_FALSE, ...
     * @param in_expr_map the map from pointer (as long int) of the expr struct to char* strings, if a mapping for eg. E_INT is defind it will take precidence over printing the value, E_VAR has to have a mapping.
     * @param in_width_map the map from pointer (as long int) of the expr struct to int for how many wires the expression is referencing to, so the width of the specifig input bus.
     * @param out_expr_list an act list for the full expression so the head of the data structure, that are outputs of to be mapped module. 
     * @param out_expr_map the map from pointer (as long int) of the expr struct to char* strings, with the name the result of the expression is assinged to.
     * @param out_width_map the map from pointer (as long int) of the expr struct to int, for the bus width of the output
     * @param hidden_expr_list optional - like out_list just the assigns are not used on the outputs, they can be used leafs for the outputs again when using the same char* string name.
     */
    ExprBlockInfo* run_external_opt (const char* expr_set_name, list_t *in_expr_list, iHashtable *in_expr_map, iHashtable *in_width_map, list_t *out_expr_list, iHashtable *out_expr_map, iHashtable *out_width_map, list_t *hidden_expr_list = NULL);


    /**
     * C-STRING MODE - set of expr - with copy on output for non unique outputs/hidden expressions
     * run_external_opt will take a set of expressions and syntesise, optimise and map it to a gate library.
     * 
     * the resulting block will be appended to the output file, the block will be named with expr_set_name,
     * In and out have to be seperate, because in the IN case you refference the expression variable/port itself and for the outputs the variable/port its assigned to and not itself.
     * the optional hidden expression list works the same way, it will source the properties for its variables from in maps, and the assing to from the out maps.
     * 
     * @param expr_set_name the name of the verilog module
     * @param in_expr_list an act list of expr leaf data structures/variables, they should be E_VAR and for bundled data additional E_INT, E_TRUE, E_FALSE, ...
     * @param in_expr_map the map from pointer (as long int) of the expr struct to char* strings, if a mapping for eg. E_INT is defind it will take precidence over printing the value, E_VAR has to have a mapping.
     * @param in_width_map the map from pointer (as long int) of the expr struct to int for how many wires the expression is referencing to, so the width of the specifig input bus.
     * @param out_expr_list an act list for the full expression so the head of the data structure, that are outputs of to be mapped module. 
     * @param out_expr_name_list an index aligned list (with regards to out_expr_list) with the name (C string pointer) the result of the expression is assinged to.
     * @param out_width_map the map from pointer (as long int) of the expr struct to int, for the bus width of the output
     * @param hidden_expr_list optional - like out_list just the assigns are not used on the outputs, they can be used leafs for the outputs again when using the same char* string name.
     * @param hidden_expr_name_list an index aligned list (with regards to hidden_expr_list) with the name (C string pointer) the result of the expression is assinged to
    */
    ExprBlockInfo* run_external_opt (const char* expr_set_name, list_t *in_expr_list, iHashtable *in_expr_map, iHashtable *in_width_map, list_t *out_expr_list, list_t *out_expr_name_list, iHashtable *out_width_map, list_t *hidden_expr_list = NULL, list_t *hidden_expr_name_list = NULL);

    /**
     * Construct a new External Exp Opt generator, supply it with all the settings needed.
     * most of the technology settings are loaded via the configuration file expropt.conf
     * 
     * @param datapath_syntesis_tool which tool to use, see expr_mapping_software
     * @param mapping_target are we mapping for bundled data or qdi
     * @param expr_file_path the output file path, this is the file all the optimised expression blocks will be saved to. if empty only metadata will be extracted.
     * @param exprid_prefix optional - the prefix for all in and outputs  - if integer id mode is used
     * @param block_prefix the prefix for the expression block - if integer id mode is used
     */
    ExternalExprOpt( const expr_mapping_software datapath_syntesis_tool,
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
                          
                          config_set_default_int ("expropt.clean_tmp_files", 1);
                          config_set_default_int ("expropt.verbose", 1);
                          config_set_default_string ("expropt.act_cell_lib_qdi_namespace", "syn");
                          config_set_default_string ("expropt.act_cell_lib_qdi_wire_type", "sdtexprchan<1>");
                          config_set_default_string ("expropt.act_cell_lib_bd_namespace", "syn");
                          config_set_default_string ("expropt.act_cell_lib_bd_wire_type", "bool");

                          config_set_default_string ("expropt.captable", "none");
                          config_set_default_string ("expropt.lef", "none");
                          config_set_default_string ("expropt.liberty_ff_hightemp", "none");
                          config_set_default_string ("expropt.liberty_ff_lowtemp", "none");
                          config_set_default_string ("expropt.liberty_ss_hightemp", "none");

			  config_set_default_real ("expropt.default_load", 1.0);

                          config_read("expropt.conf");

                          if (wire_encoding == qdi){
                            cell_act_file = config_get_string("expropt.act_cell_lib_qdi");
                            cell_namespace = config_get_string("expropt.act_cell_lib_qdi_namespace");
                            expr_channel_type = config_get_string("expropt.act_cell_lib_qdi_wire_type");
                          }
                          else
                          {
                            cell_act_file = config_get_string("expropt.act_cell_lib_bd");
                            cell_namespace = config_get_string("expropt.act_cell_lib_bd_namespace");
                            expr_channel_type = config_get_string("expropt.act_cell_lib_bd_wire_type");
                          }
                          cleanup = config_get_int("expropt.clean_tmp_files"); 
                      }
    ~ExternalExprOpt();

    /**
     * work in progress value extraction from genus logs, area is also possible in abc, @todo abc timing is not known if printable.
     * 
     * @param log_file_name the log file, acutally the base name the individual reports are speperate for each case
     * @return ExprBlockInfo* the datastructure with the result data
     */
    ExprBlockInfo* parse_genus_log(std::string log_file_name);

private:

    bool cleanup;
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
    void print_expr_verilog (FILE *output_stream, const char* expr_set_name, list_t *in_list,  iHashtable *inexprmap, iHashtable *inwidthmap, list_t *out_list, list_t *out_name_list, iHashtable *outwidthmap, list_t *expr_list = NULL, list_t *hidden_name_list = NULL);
    
    /**
     * the generator for the genus run scripts.
     * 
     * @param tcl_file_name the file the genus instructions are written to
     * @param file_name the file name of the input verilog.
     * @param out_file_name the file for the mapped output file.
     * @param process_name the name of the top level verilog module.
     */
    void generate_genus_tcl(const char *tcl_file_name, const char *file_name, const char *out_file_name, const char* process_name);

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



    double parse_and_return_max(std::string filename, std::string parse_format, double failure_value = 0 , bool fail_if_file_does_no_exist = false);


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
    const expr_mapping_software mapper;

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

    /**
     * This must be an E_VAR
     */
    std::unordered_map<Expr *, int> _varwidths;
    int _get_bitwidth (Expr *e);

};
#endif



#pragma once
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

#include <act/act.h>
#include <string>
#include <unordered_map>
#include <vector>

/// enum for referencing the mapper software type, to define which external synthesis tool to use for synthesis
enum expr_mapping_software { yosys = 0, synopsis = 1, genus = 2 };
enum class ExprMappingSoftware { yosys, synopsis, genus };

/// enum for referencing the pass type
enum expr_mapping_target { qdi = 0, bd = 1 };
enum class ExprMappingTarget { qdi, bd };

enum class ShouldTieCells { no, yes };

/// the metadata object holds all extracted points of the expr set.
class ExprBlockInfo {
  public:
    /// the typical delay corner as normal temp, in seconds. 0 if not extracted.
    const double delay_typ;

    /// the max delay corner as normal high and slow slow corner, in seconds. 0 if not extracted.
    const double delay_max;

    /// the min delay corner as low temp and fast fast corner, in seconds. 0 if not extracted.
    const double delay_min;

    /// the typical power corner at normal temp, in A?. 0 if not extracted.
    const double power_typ;

    /// the typical power corner at normal temp, in A?. 0 if not extracted.
    const double power_typ_static;

    /// the typical power corner at normal temp, in A?. 0 if not extracted.
    const double power_typ_dynamic;

    /// the high power corner at high temp and fast fast corner, in A?. 0 if not extracted.
    const double power_max;

    /// the typical power corner at normal temp, in A?. 0 if not extracted.
    const double power_max_static;

    /// the typical power corner at normal temp, in A?. 0 if not extracted.
    const double power_max_dynamic;

    /// the theoretical area of all gates combined, with 100% utilisation. 0 if not extracted.
    const double area;

    /// Construct a new Expr Block Info object, values can not be changed after creation. parameter see getter
    /// descriptions.
    ExprBlockInfo(const double e_delay_typ, const double e_delay_min, const double e_delay_max,
                  const double e_power_typ, const double e_power_max, const double e_area,
                  const double e_power_typ_static = 0, const double e_power_typ_dynamic = 0,
                  const double e_power_max_static = 0, const double e_power_max_dynamic = 0)
        : delay_typ(e_delay_typ)
        , delay_max(e_delay_max)
        , delay_min(e_delay_min)
        , power_typ(e_power_typ)
        , power_typ_static(e_power_typ_static)
        , power_typ_dynamic(e_power_typ_dynamic)
        , power_max(e_power_max)
        , power_max_static(e_power_max_static)
        , power_max_dynamic(e_power_max_dynamic)
        , area(e_area) {}

    /// Construct a new Expr Block dummy with no extration results =>
    /// all 0, but delay_typ = -1 to indicate that the results were not
    /// created.
    ExprBlockInfo()
        : delay_typ(-1)
        , delay_max(0)
        , delay_min(0)
        , power_typ(0)
        , power_typ_static(0)
        , power_typ_dynamic(0)
        , power_max(0)
        , power_max_static(0)
        , power_max_dynamic(0)
        , area(0) {}
    ~ExprBlockInfo() = default;
};

struct ExprPtrWithNameAndWidth {
    const Expr *e;
    std::string name;
    int width;
};
struct ExprPtrWithIdAndWidth {
    const Expr *e;
    int id;
    int width;
};

/**
 * ExternalExprOpt is an interface that wrapps the syntesis, optimisation and mapping to cells of a set of act expr.
 * it will also give you metadata back if the software supports it.
 * if you
 */
class ExternalExprOpt {
  public:
    /// deprecated. Use new interface (probably the one below)
    [[maybe_unused]] [[nodiscard]] ExprBlockInfo *
    run_external_opt(int expr_set_number, int targetwidth, const Expr *e, list_t *in_expr_list, iHashtable *in_expr_map,
                     iHashtable *in_width_map) const;

    /// Given an expression `e`, this will compute an optimized set of gates to compute `e` (resized to final width
    /// `target_width`), and append the set of gates to the output file. The generated block of gates will be named
    /// "<block_prefix><expr_set_number>" and the output will have name "out". All inputs will be named
    /// "<expr_prefix><expr_id>"
    /// @param expr the expression to be optimised and mapped
    /// @param target_width what output width `e` should have
    /// @param leafs an act list of expr leaf data structures/variables. Every expression of type E_VAR should be
    /// present. Optionally, expressions of type E_INT, E_TRUE, and E_FALSE should also be present.
    [[maybe_unused]] [[nodiscard]] ExprBlockInfo *
    run_external_opt(int expr_set_number, const Expr *expr, int target_width,
                     const std::vector<ExprPtrWithIdAndWidth> &leafs) const;

    /// deprecated. Use a new interface
    [[maybe_unused]] [[nodiscard]] ExprBlockInfo *
    run_external_opt(int expr_set_number, list_t *expr_list, list_t *in_list, list_t *out_list,
                     iHashtable *exprmap) const;

    /// Use one of the other interfaces. This one is bad!
    [[maybe_unused]] [[nodiscard]] ExprBlockInfo *
    run_external_opt(int expr_set_number, const std::vector<std::pair<int, int>> &in_list,
                     const std::vector<std::pair<int, int>> &out_list,
                     const std::unordered_map<const Expr *, int> &exprmap_int) const;

    /// deprecated. Use new interface (probably the one below)
    [[maybe_unused]] [[nodiscard]] ExprBlockInfo *
    run_external_opt(const char *expr_set_name, list_t *in_expr_list, iHashtable *in_expr_map, iHashtable *in_width_map,
                     list_t *out_expr_list, iHashtable *out_expr_map, iHashtable *out_width_map,
                     list_t *hidden_expr_list = nullptr) const;

    /// deprecated. Use new interface (probably the one below)
    [[maybe_unused]] [[nodiscard]] [[nodiscard]] ExprBlockInfo *
    run_external_opt(const char *expr_set_name, list_t *in_expr_list, iHashtable *in_expr_map, iHashtable *in_width_map,
                     list_t *out_expr_list, list_t *out_expr_name_list, iHashtable *out_width_map,
                     list_t *hidden_expr_list = nullptr, list_t *hidden_expr_name_list = nullptr) const;

    /// This will generate a series of gates to compute the expressions in `out_exprs.` It will do this using the inputs
    /// in `leafs`, and by computing the intermediate values in `hidden_exprs`. It will copy out_exprs and hidden_exprs
    /// with repeated names The name of the block of generated gates will be `expr_set_name.` The names in `leafs`,
    /// `out_exprs` and `hidden_exprs` must be mutually disjoint.
    [[maybe_unused]] [[nodiscard]] ExprBlockInfo *
    run_external_opt(const std::string &expr_set_name, const std::vector<ExprPtrWithNameAndWidth> &leafs,
                     const std::vector<ExprPtrWithNameAndWidth> &out_exprs,
                     const std::vector<ExprPtrWithNameAndWidth> &hidden_exprs) const;

    /// deprecated. Use the new interface. Will be removed in a future version.
    ExternalExprOpt(expr_mapping_software datapath_synthesis_tool,
                                                            expr_mapping_target mapping_target, bool tie_cells,
                                                            std::string expr_file_path = "",
                                                            std::string exprid_prefix = "e",
                                                            std::string block_prefix = "blk")
        : ExternalExprOpt(
              (datapath_synthesis_tool == expr_mapping_software::genus
                   ? ExprMappingSoftware::genus
                   : (datapath_synthesis_tool == expr_mapping_software::synopsis ? ExprMappingSoftware::synopsis
                                                                                 : ExprMappingSoftware::yosys)),
              (mapping_target == expr_mapping_target::bd ? ExprMappingTarget::bd : ExprMappingTarget::qdi),
              tie_cells ? ShouldTieCells::yes : ShouldTieCells::no, std::move(expr_file_path), std::move(exprid_prefix),
              std::move(block_prefix)) {}

    /// @param datapath_synthesis_tool which optimization tool to use. Right now we support yosys, synopsis, and genus
    /// @param mapping_target are we mapping for bundled data or qdi
    /// @param tie_cells TODO I (Henry) am not sure what this does
    /// @param expr_file_path the output file path, this is the file all the optimised expression blocks will be saved
    /// to. if empty only metadata will be extracted.
    /// @param exprid_prefix optional - the prefix for all in and outputs  - if integer id mode is used
    /// @param block_prefix the prefix for the expression block - if integer id mode is used
    ExternalExprOpt(ExprMappingSoftware datapath_synthesis_tool, ExprMappingTarget mapping_target,
                    ShouldTieCells tie_cells, std::string expr_file_path = "", std::string exprid_prefix = "e",
                    std::string block_prefix = "blk");
    ~ExternalExprOpt() = default;

    //  TODO abc timing is not known if printable.
    /// work in progress value extraction from genus logs, area is also possible in abc
    /// @param log_file_name the log file, actually the base name the individual reports are separate for each case
    /// @return ExprBlockInfo* the datastructure with the result data
    [[nodiscard]] static ExprBlockInfo *parse_genus_log(const std::string &log_file_name);

  private:
    bool cleanup;

    /// the output file name where all act results are appended too.
    std::string expr_output_file;

    /// the act cell lib read from the config file
    std::string cell_act_file;

    /// the name space of the cell act file, read from the config file the default is "syn"
    std::string cell_namespace;

    /// what wire type is used in v2act, if bool is chosen v2act will act in sync mode for all other it runs in async
    /// mode. default is "bool" for bd and "dualrail" for qdi
    std::string expr_channel_type;

    /// if used in the integer mode the prefix for each expression in and out.
    /// default is "e"
    std::string expr_prefix;

    /// the module prefeix when used in integer mode. default is "blk"
    std::string module_prefix;

    /// the software to be used for syntesis and mapping.
    ExprMappingSoftware mapper;

    /// should tie cells be incerted by the syntesis software (true), or should v2act take cae of it (false)
    ShouldTieCells use_tie_cells;

    /// is it a bundeld data or dualrail pass?
    ExprMappingTarget wire_encoding;
};

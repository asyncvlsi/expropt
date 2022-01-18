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

#include "expropt.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <list>
#include <memory>
#include <vector>

#ifdef FOUND_exproptcommercial
#include <act/exproptcommercial.h>
#endif

namespace {
template <typename T> std::vector<T> to_vector(list_t *l) {
    static_assert(std::is_pointer_v<T>);
    std::vector<T> result;
    for (auto li = list_first(l); li; li = list_next(li))
        result.push_back((T) const_cast<void *> list_value(li));
    return result;
}

std::vector<std::pair<int, int>> to_ipair_vector(list_t *l) {
    std::vector<std::pair<int, int>> result;
    for (auto li = list_first(l); li; li = list_next(li)) {
        auto t1 = list_ivalue(li);
        li = list_next(li);
        auto t2 = list_ivalue(li);
        result.emplace_back(t1, t2);
    }
    return result;
}

template <typename K, typename V> std::unordered_map<K, V> to_map(iHashtable *table) {
    static_assert(std::is_pointer_v<V>);
    static_assert(std::is_pointer_v<K>);
    std::unordered_map<K, V> result;
    ihash_iter_t iter;
    ihash_bucket_t *ib;
    ihash_iter_init(table, &iter);
    while ((ib = ihash_iter_next(table, &iter))) {
        result[(K)ib->key] = (V)ib->v;
    }
    return result;
}

template <typename K, typename V> std::unordered_map<K, int> to_imap(iHashtable *table) {
    static_assert(std::is_pointer_v<K>);
    static_assert(std::is_same_v<V, int>);
    std::unordered_map<K, int> result;
    ihash_iter_t iter;
    ihash_bucket_t *ib;
    ihash_iter_init(table, &iter);
    while ((ib = ihash_iter_next(table, &iter))) {
        result[(K)ib->key] = ib->i;
    }
    return result;
}

template <typename T, typename T1, typename T2, typename T3>
std::vector<T> zip_vmm(const std::vector<T1> &v1, const std::unordered_map<T1, T2> &v2,
                       const std::unordered_map<T1, T3> &v3) {
    std::vector<T> u;
    u.reserve(v1.size());
    for (const auto &x : v1)
        u.push_back(T{x, v2.at(x), v3.at(x)});
    return u;
}

template <typename T, typename T1, typename T2, typename T3>
std::vector<T> zip_vvm(const std::vector<T1> &v1, const std::vector<T2> &v2, const std::unordered_map<T1, T3> &v3) {
    std::vector<T> u;
    u.reserve(v1.size());
    assert(v1.size() == v2.size());
    for (ssize_t i = 0; i != (ssize_t)v1.size(); ++i)
        u.push_back(T{v1[i], v2[i], v3.at(v1[i])});
    return u;
}

template <typename T, typename K, typename T1, typename T2>
std::unordered_map<K, T> zip_mm(const std::unordered_map<K, T1> &v1, const std::unordered_map<K, T2> &v2) {
    std::unordered_map<K, T> u;
    for (const auto &[k, _]: v1) assert(v2.contains(k));
    for (const auto &[k, _]: v2) assert(v1.contains(k));
    u.reserve(v1.size());
    for (const auto &[k, t1]: v1)
        u[k] = T{t1, v2.at(k)};
    return u;
}

template <typename T, typename K, typename V>
std::unordered_map<K, T> map_val_remap(const std::unordered_map<K, V> &m) {
    std::unordered_map<K, T> u;
    for (const auto &[k, v] : m)
        u[k] = T{v};
    return u;
}

template <typename T, typename V> std::vector<T> vec_val_remap(const std::vector<V> &v) {
    std::vector<T> u;
    u.reserve(v.size());
    for (const auto &x : v)
        u.push_back(T{x});
    return u;
}

// https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
template <typename... Args> [[nodiscard]] std::string string_format(const std::string &format, Args... args) {
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
    if (size_s <= 0) {
        throw std::runtime_error("Error during formatting.");
    }
    auto size = static_cast<size_t>(size_s);
    auto buf = std::make_unique<char[]>(size);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return {buf.get(), buf.get() + size - 1}; // We don't want the '\0' inside
}

/*
 * the recusive print method for the act expression data structure. it will use the hashtable expression map to get the
 * IDs for the variables and constants.
 *
 * for the constants they can be either defined in the exprmap, than they are printed as inputs (for dualrail systems)
 * or if they are not in the exprmap, they will be printed as a constant in verilog for the tool to optimise and map to
 * tiecells (for bundled data)
 *
 * the keys for the exprmaps are the pointers of the e of type E_VAR and optinal of E_INT, E_TRUE, E_FALSE (or E_REAL)
 * for dualrail. if a mapping exsists for these leaf types the mapping will be prefered over printing the value.
 *
 * name_from_leaf Entries are required for E_VAR and E_BITFIELD, and are optional for E_INT, E_TRUE, and E_FALSE. If a
 * value is present for E_INT, E_TRUE, or E_FALSE, it will be used instead of the value stored in the expression.
 */
void print_expression(FILE *output_stream, const Expr *e,
                      const std::unordered_map<const Expr *, const char *> &exprmap) {
    fprintf(output_stream, "(");
    switch (e->type) {
    case E_BUILTIN_BOOL:
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " == 0 ? 1'b0 : 1'b1");
        break;

    case E_BUILTIN_INT:
        if (!e->u.e.r) {
            print_expression(output_stream, e->u.e.l, exprmap);
        } else {
            int v;
            print_expression(output_stream, e->u.e.l, exprmap);
            fprintf(output_stream, " & ");
            v = e->u.e.r->u.v;
            fprintf(output_stream, "%d'b", v);
            for (int i = 0; i < v; i++) {
                fprintf(output_stream, "1");
            }
        }
        break;

    case (E_AND):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " & ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_OR):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " | ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_NOT):
        fprintf(output_stream, " ~");
        print_expression(output_stream, e->u.e.l, exprmap);
        break;
    case (E_PLUS):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " + ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_MINUS):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " - ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_MULT):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " * ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_DIV):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " / ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_MOD):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " %% ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_LSL):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " << ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_LSR):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " >> ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_ASR):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " >>> ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_UMINUS):
        fprintf(output_stream, " (-");
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, ")");
        break;
    case (E_INT): {
        auto b = exprmap.find(e);
        if (b != exprmap.end())
            fprintf(output_stream, "%s", b->second);
        else {
            fprintf(output_stream, "64'd%lu", e->u.v);
        }
    }

    break;
    case (E_VAR): {
        fprintf(output_stream, "%s", exprmap.at(e));
    } break;
    case (E_QUERY):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " ? ");
        print_expression(output_stream, e->u.e.r->u.e.l, exprmap);
        fprintf(output_stream, " : ");
        print_expression(output_stream, e->u.e.r->u.e.r, exprmap);
        break;
    case (E_LPAR):
        fprintf(output_stream, "LPAR\n");
        fatal_error("%u not implemented", e->type);
        break;
    case (E_RPAR):
        fprintf(output_stream, "RPAR\n");
        fatal_error("%u not implemented", e->type);
        break;
    case (E_XOR):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " ^ ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_LT):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " < ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_GT):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " > ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_LE):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " <=");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_GE):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " >= ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_EQ):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " == ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_NE):
        print_expression(output_stream, e->u.e.l, exprmap);
        fprintf(output_stream, " != ");
        print_expression(output_stream, e->u.e.r, exprmap);
        break;
    case (E_TRUE): {
        auto b = exprmap.find(e);
        if (b != exprmap.end())
            fprintf(output_stream, "%s", b->second);
        else
            fprintf(output_stream, " 1'b1 ");
    } break;
    case (E_FALSE): {
        auto b = exprmap.find(e);
        if (b != exprmap.end())
            fprintf(output_stream, "%s", b->second);
        else
            fprintf(output_stream, " 1'b0 ");
    } break;
    case (E_COLON):
        fprintf(output_stream, " : ");
        break;
    case (E_PROBE):
        fprintf(output_stream, "PROBE");
        fatal_error("%u should have been handled else where", e->type);
        break;
    case (E_COMMA):
        fprintf(output_stream, "COMMA");
        fatal_error("%u should have been handled else where", e->type);
        break;
    case (E_CONCAT):
        if (!e->u.e.r) {
            print_expression(output_stream, e->u.e.l, exprmap);
        } else {
            const Expr *tmp = e;
            fprintf(output_stream, "{");
            while (tmp) {
                print_expression(output_stream, tmp->u.e.l, exprmap);
                if (tmp->u.e.r) {
                    fprintf(output_stream, " ,");
                }
                tmp = tmp->u.e.r;
            }
            fprintf(output_stream, "}");
        }
        break;
    case (E_BITFIELD):
        unsigned int l;
        unsigned int r;
        if (e->u.e.r->u.e.l) {
            l = (unsigned long)e->u.e.r->u.e.r->u.v;
            r = (unsigned long)e->u.e.r->u.e.l->u.v;
        } else {
            l = (unsigned long)e->u.e.r->u.e.r->u.v;
            r = l;
        }

        { fprintf(output_stream, "%s", exprmap.at(e)); }
        fprintf(output_stream, " [");
        if (l != r) {
            fprintf(output_stream, "%i:", l);
            fprintf(output_stream, "%i", r);
        } else {
            fprintf(output_stream, "%i", r);
        }
        fprintf(output_stream, "]");
        break;
    case (E_COMPLEMENT):
        fprintf(output_stream, " ~");
        print_expression(output_stream, e->u.e.l, exprmap);
        break;
    case (E_REAL): {
        auto b = exprmap.find(e);
        if (b != exprmap.end())
            fprintf(output_stream, "%s", b->second);
        else
            fprintf(output_stream, "64'd%lu", e->u.v);
    } break;
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
    fprintf(output_stream, ")");
}

// print a verilog module that will compute the Expr* in `out_exprs` given the `leaf_exprs` as inputs, using
// `hidden_exprs` as intermediate values.
void print_expr_verilog(FILE *output_stream, const std::string &expr_set_name,
                        const std::vector<const Expr *> &input_exprs,
                        const std::unordered_map<const Expr *, NameWithWidth> &leaf_map,
                        const std::vector<ExprPtrWithNameAndWidth> &out_exprs,
                        const std::vector<ExprPtrWithNameAndWidth> &hidden_exprs) {
    if (!output_stream)
        fatal_error("ExternalExprOpt::print_expr_verilog: verilog file is not writable");

    // first, build the lookup table for input expression names which will be used by the "print_expression" function.
    // The strings are kept alive in the original maps, so this is ok.
    std::unordered_map<const Expr *, const char *> name_from_leaf;
    for (const auto &[leaf, nw] : leaf_map) {
        name_from_leaf[leaf] = nw.name.c_str();
    }

    // Then output the verilog, starting with the module header
    fprintf(output_stream, "// generated expression module for %s\n\n\n", expr_set_name.c_str());
    fprintf(output_stream, "module %s (", expr_set_name.c_str());

    // now the ports for the header - first inputs
    bool first = true;

    for (auto li_it = input_exprs.begin(); li_it != input_exprs.end(); ++li_it) {
        if (first)
            first = false;
        else
            fprintf(output_stream, ", ");
        std::string current = leaf_map.at(*li_it).name;
        bool skip = false;
        // omit ports with the same name
        for (auto search_it = std::next(li_it); search_it != input_exprs.end(); ++search_it) {
            std::string search = leaf_map.at(*search_it).name;
            if (current == search) {
                skip = true;
                break;
            }
        }
        if (!skip)
            fprintf(output_stream, "%s", current.data());
    }

    // now the ports for the header  - than outputs
    {
        for (auto li = out_exprs.begin(); li != out_exprs.end(); ++li) {
            if (first)
                first = false;
            else
                fprintf(output_stream, ", ");
            std::string current = li->name;
            bool skip = false;
            // omit ports with the same name
            for (auto li_search = std::next(li); li_search != out_exprs.end(); ++li_search) {
                std::string search = li_search->name;
                if (current == search) {
                    skip = true;
                    break;
                }
            }
            if (!skip)
                fprintf(output_stream, "%s", current.data());
        }
        fprintf(output_stream, " );\n");
    }

    // print input ports with bitwidth
    fprintf(output_stream, "\n\t// print input ports with bitwidth\n");
    for (auto li = input_exprs.begin(); li != input_exprs.end(); ++li) {
        // make sure you dont print a port 2+ times - the tools really dont like that
        std::string current = leaf_map.at(*li).name;
        bool skip = false;
        for (auto li_search = std::next(li); li_search != input_exprs.end(); li_search = std::next(li_search)) {
            std::string search = leaf_map.at(*li_search).name;
            if (current == search) {
                skip = true;
                break;
            }
        }
        if (skip)
            continue;
        // look up the bitwidth
        int width = leaf_map.at(*li).width;
        if (width <= 0)
            fatal_error(
                "ExternalExprOpt::print_expr_verilog error: Expression operands have incompatible bit widths\n");
        else if (width == 1)
            fprintf(output_stream, "\tinput %s ;\n", current.data());
        else
            fprintf(output_stream, "\tinput [%i:0] %s ;\n", width - 1, current.data());
    }

    // print output ports with bitwidth
    {
        fprintf(output_stream, "\n\t// print output ports with bitwidth\n");
        for (auto li = out_exprs.begin(); li != out_exprs.end(); ++li) {
            // make sure you dont print a port 2+ times - the tools really dont like that
            std::string current = li->name;
            bool skip = false;
            // omit ports with the same name
            for (auto li_search = std::next(li); li_search != out_exprs.end(); ++li_search) {
                std::string search = li_search->name;
                if (current == search) {
                    skip = true;
                    break;
                }
            }
            if (skip)
                continue;
            // look up the bitwidth
            int width = li->width;
            if (width <= 0)
                fatal_error(
                    "chpexpr2verilog::print_expr_set error: Expression operands have incompatible bit widths\n");
            else if (width == 1)
                fprintf(output_stream, "\toutput %s ;\n", current.data());
            else
                fprintf(output_stream, "\toutput [%i:0] %s ;\n", width - 1, current.data());
        }
    }

    // the hidden logic statements
    if (!hidden_exprs.empty()) {
        fprintf(output_stream, "\n\t// the hidden logic vars declare\n");
        for (auto li = hidden_exprs.begin(); li != hidden_exprs.end(); ++li) {
            std::string current = li->name;
            bool skip = false;
            // omit ports with the same name
            for (auto li_search = std::next(li); li_search != hidden_exprs.end(); ++li_search) {
                std::string search = li_search->name;
                if (current == search) {
                    skip = true;
                    break;
                }
            }
            if (skip)
                continue;
            // the bitwidth
            int width = li->width;
            if (width <= 0)
                fatal_error(
                    "chpexpr2verilog::print_expr_set error: Expression operands have incompatible bit widths\n");
            else if (width == 1)
                fprintf(output_stream, "\twire %s ;\n", current.data());
            else
                fprintf(output_stream, "\twire [%i:0] %s ;\n", width - 1, current.data());
        }
    }

    // the hidden logic statements
    if (!hidden_exprs.empty()) {
        fprintf(output_stream, "\n\t// the hidden logic statements as assigns\n");

        for (auto li = hidden_exprs.begin(); li != hidden_exprs.end(); ++li) {
            const Expr *e = li->e;
            std::string current = li->name;
            bool skip = false;
            // omit ports with the same name
            for (auto li_search = std::next(li); li_search != hidden_exprs.end(); ++li_search) {
                std::string search = li_search->name;
                if (current == search) {
                    skip = true;
                    break;
                }
            }
            if (skip)
                continue;
            // also print the hidden assigns
            fprintf(output_stream, "\tassign %s = ", current.data());
            print_expression(output_stream, e, name_from_leaf);
            fprintf(output_stream, ";\n");
        }
    }

    // the actual logic statements
    {
        fprintf(output_stream, "\n\t// the actuall logic statements as assigns\n");
        for (const auto &out_expr : out_exprs) {
            const Expr *e = out_expr.e;
            std::string current = out_expr.name;
            fprintf(output_stream, "\tassign %s = ", current.data());
            print_expression(output_stream, e, name_from_leaf);
            fprintf(output_stream, ";\n");
        }
        fprintf(output_stream, "\nendmodule\n");
    }
}

double parse_and_return_max(std::string filename, std::string parse_format, double failure_value,
                            bool fail_if_file_does_no_exist = false) {
    std::ifstream log_stream;
    log_stream.open(filename);
    if (!log_stream.is_open()) {
        if (fail_if_file_does_no_exist)
            fatal_error("could not open file %s for reading", filename.data());
        return failure_value;
    }
    std::string current_line;
    double return_value = failure_value;
    double value_buffer = 0;
    bool first = true;
    while (std::getline(log_stream, current_line)) {
        if (sscanf(current_line.data(), parse_format.data(), &value_buffer) == 1) {
            if (first) {
                return_value = value_buffer;
                first = false;
            } else if (value_buffer > return_value)
                return_value = value_buffer;
        }
    }
    log_stream.close();
    return return_value;
}

} // namespace

ExternalExprOpt::ExternalExprOpt(ExprMappingSoftware datapath_syntesis_tool, ExprMappingTarget mapping_target,
                                 ShouldTieCells tie_cells, std::string expr_file_path, std::string exprid_prefix,
                                 std::string block_prefix)
    : expr_output_file(std::move(expr_file_path))
    , expr_prefix(std::move(exprid_prefix))
    , module_prefix(std::move(block_prefix))
    , mapper(datapath_syntesis_tool)
    , use_tie_cells(tie_cells)
    , wire_encoding(mapping_target) {

    config_set_default_int("expropt.clean_tmp_files", 1);
    config_set_default_int("expropt.verbose", 1);
    config_set_default_string("expropt.act_cell_lib_qdi_namespace", "syn");
    config_set_default_string("expropt.act_cell_lib_qdi_wire_type", "sdtexprchan<1>");
    config_set_default_string("expropt.act_cell_lib_bd_namespace", "syn");
    config_set_default_string("expropt.act_cell_lib_bd_wire_type", "bool");

    config_set_default_string("expropt.captable", "none");
    config_set_default_string("expropt.lef", "none");
    config_set_default_string("expropt.liberty_ff_hightemp", "none");
    config_set_default_string("expropt.liberty_ff_lowtemp", "none");
    config_set_default_string("expropt.liberty_ss_hightemp", "none");

    config_set_default_real("expropt.default_load", 1.0);

    config_read("expropt.conf");

    switch (wire_encoding) {
    case ExprMappingTarget::qdi:
        cell_act_file = config_get_string("expropt.act_cell_lib_qdi");
        cell_namespace = config_get_string("expropt.act_cell_lib_qdi_namespace");
        expr_channel_type = config_get_string("expropt.act_cell_lib_qdi_wire_type");
        break;
    case ExprMappingTarget::bd:
        cell_act_file = config_get_string("expropt.act_cell_lib_bd");
        cell_namespace = config_get_string("expropt.act_cell_lib_bd_namespace");
        expr_channel_type = config_get_string("expropt.act_cell_lib_bd_wire_type");
        break;
    }
    cleanup = config_get_int("expropt.clean_tmp_files");
}

// the a wrapper for chp2prs to just run the optimisation with a single expression
[[maybe_unused]] ExprBlockInfo *ExternalExprOpt::run_external_opt(int expr_set_number, int target_width,
                                                                  const Expr *expr, list_t *in_expr_list,
                                                                  iHashtable *in_expr_map,
                                                                  iHashtable *in_width_map) const {
    auto leafs =
        zip_vmm<ExprPtrWithIdAndWidth>(to_vector<const Expr *>(in_expr_list), to_imap<const Expr *, int>(in_expr_map),
                                       to_imap<const Expr *, int>(in_width_map));
    return run_external_opt(expr_set_number, expr, target_width, leafs);
}
ExprBlockInfo *ExternalExprOpt::run_external_opt(int expr_set_number, const Expr *expr, int target_width,
                                                 const std::vector<ExprPtrWithIdAndWidth> &leafs) const {
    std::vector<const Expr *> input_exprs;
    std::unordered_map<const Expr *, NameWithWidth> leaf_map;
    for (const auto &[e, id, width] : leafs) {
        input_exprs.push_back(e);
        leaf_map[e] = {string_format("%s%u", expr_prefix.data(), id), width};
    }
    auto out_exprs = std::vector<ExprPtrWithNameAndWidth>{{expr, "out", target_width}};
    auto expr_set_name = string_format("%s%u", module_prefix.data(), expr_set_number);

    return run_external_opt(expr_set_name, input_exprs, leaf_map, out_exprs, {});
}

// the wrapper for chp2prs to run sets of expressions like guards, uses chp2prs data structures and converts them to ExprOpt standart
[[maybe_unused]] ExprBlockInfo *ExternalExprOpt::run_external_opt(int expr_set_number, list_t * /*expr_list*/,
                                                                  list_t *in_list, list_t *out_list,
                                                                  iHashtable *exprmap_int) const {
    return run_external_opt(expr_set_number, to_ipair_vector(in_list), to_ipair_vector(out_list),
                            to_imap<const Expr *, int>(exprmap_int));
}
ExprBlockInfo *ExternalExprOpt::run_external_opt(int expr_set_number, const std::vector<std::pair<int, int>> &in_list,
                                                 const std::vector<std::pair<int, int>> &out_list,
                                                 const std::unordered_map<const Expr *, int> &exprmap_int) const {

    // Note: all the values in exprmap_int must be unique
    std::unordered_map<int, const Expr *> exprmap_int_inv;
    for (auto &[k, v] : exprmap_int) {
        assert(!exprmap_int_inv.contains(v));
        exprmap_int_inv[v] = k;
    }

    std::vector<const Expr *> input_exprs;
    std::unordered_map<const Expr *, NameWithWidth> leaf_map;
    for (const auto &[li, li_width] : in_list) {
        const Expr *e = exprmap_int_inv.at(li);
        assert(e);
        input_exprs.push_back(e);
        leaf_map[e] = {string_format("%s%u", expr_prefix.data(), li), li_width};
    }

    std::vector<ExprPtrWithNameAndWidth> out_exprs;
    for (const auto &[li, li_width] : out_list) {
        const Expr *e = exprmap_int_inv.at(li);
        assert(e);
        out_exprs.push_back({e, string_format("%s%u", expr_prefix.data(), li), li_width});
    }
    // generate module name
    auto expr_set_name = string_format("%s%u", module_prefix.data(), expr_set_number);

    return run_external_opt(expr_set_name, input_exprs, leaf_map, out_exprs, {});
}

/**
 * first constuct the filenames for the temporary files and than generate the verilog, exc the external tool, read out
 * the results and convert them back to act.
 *
 * the printing of the verilog is seperate
 */
[[maybe_unused]] ExprBlockInfo *ExternalExprOpt::run_external_opt(const char *expr_set_name, list_t *c_in_exprs,
                                                                  iHashtable *c_in_name_map, iHashtable *c_in_width_map,
                                                                  list_t *c_out_exprs, iHashtable *c_out_name_map,
                                                                  iHashtable *c_out_width_map,
                                                                  list_t *c_hidden_exprs) const {
    // widths and names for the hidden expressions are in the c_out_width_map map
    auto input_name_map = map_val_remap<std::string>(to_map<const Expr *, const char *>(c_in_name_map));
    auto input_width_map = to_imap<const Expr *, int>(c_in_width_map);

    auto input_exprs =to_vector<const Expr *>(c_in_exprs);

    auto leaf_map = zip_mm<NameWithWidth>(input_name_map, input_width_map);

    auto out_width_map = to_imap<const Expr *, int>(c_out_width_map);
    auto out_name_map = map_val_remap<std::string>(to_map<const Expr *, const char *>(c_out_name_map));
    auto out_exprs =
        zip_vmm<ExprPtrWithNameAndWidth>(to_vector<const Expr *>(c_out_exprs), out_name_map, out_width_map);
    auto hidden_exprs =
        zip_vmm<ExprPtrWithNameAndWidth>(to_vector<const Expr *>(c_hidden_exprs), out_name_map, out_width_map);

    return run_external_opt(expr_set_name, input_exprs, leaf_map, out_exprs, hidden_exprs);
}

namespace {
double parse_abc_info(const char *file, double *area) {
    char buf[10240];
    FILE *fp;
    double ret;

    // This is a hack, fix this later...
    // Taken from OSU .lib file
    struct cell_info {
        const char *name;
        double area;
        int count;
    };
    cell_info cell_infos[] = {
        {"AND2X1", 32, 0}, {"AND2X2", 32, 0}, {"AOI21X1", 32, 0}, {"AND2X1", 32, 0},   {"AOI22X2", 40, 0},
        {"BUFX2", 24, 0},  {"BUFX4", 32, 0},  {"CLKBUF1", 72, 0}, {"CLKBUF2", 104, 0}, {"CLKBUF3", 136, 0},
        {"FAX1", 120, 0},  {"HAX1", 80, 0},   {"INVX1", 16, 0},   {"INVX2", 16, 0},    {"INVX4", 24, 0},
        {"INVX8", 40, 0},  {"LATCH", 16, 0},  {"MUX2X1", 48, 0},  {"NAND2X1", 24, 0},  {"NAND3X1", 36, 0},
        {"NOR2X1", 24, 0}, {"NOR3X1", 64, 0}, {"OAI21X1", 24, 0}, {"OAI22X1", 40, 0},  {"OR2X1", 32, 0},
        {"OR2X2", 32, 0},  {"TBUFX1", 40, 0}, {"TBUFX2", 56, 0},  {"XNOR2X1", 56, 0},  {"XOR2X1", 56, 0}};

    snprintf(buf, 10240, "%s.log", file);
    fp = fopen(buf, "r");
    if (!fp) {
        return -1;
    }

    ret = -1;
    for (auto &i : cell_infos) {
        i.count = 0;
    }

    while (fgets(buf, 10240, fp)) {
        if (strncmp(buf, "ABC:", 4) == 0) {
            char *tmp = strstr(buf, "Delay =");
            if (tmp) {
                if (sscanf(tmp, "Delay = %lf ps", &ret) == 1) {
                    ret = ret * 1e-12;
                }
            }
        } else if (strncmp(buf, "ABC RESULTS:", 12) == 0) {
            char *tmp = buf + 12;
            while (*tmp && isspace(*tmp)) {
                tmp++;
            }
            if (*tmp) {
                char *cell_name = tmp;
                while (*tmp && !isspace(*tmp)) {
                    tmp++;
                }
                if (*tmp) {
                    *tmp = '\0';
                    tmp++;
                }
                int count = -1;
                if (strncmp(tmp, "cells:", 6) == 0) {
                    tmp += 6;
                    if (sscanf(tmp, "%d", &count) != 1) {
                        count = -1;
                    }
                }
                if (*tmp && count > 0) {
                    int i;
                    for (i = 0; i < (ssize_t)(sizeof(cell_infos) / sizeof(cell_infos[0])); i++) {
                        if (strcmp(cell_name, cell_infos[i].name) == 0) {
                            cell_infos[i].count++;
                            break;
                        }
                    }
                    // printf ("got cell %s, count = %d\n", cell_name, count);
                    /* got cell count! */
                    /* XXX: the area is in the .lib file... */
                }
            }
        }
    }
    if (area) {
        int i;
        *area = 0;
        for (i = 0; i < (ssize_t)(sizeof(cell_infos) / sizeof(cell_infos[0])); i++) {
            *area += cell_infos[i].area * cell_infos[i].count;
        }
    }
    return ret;
}
} // namespace

/**
 * first constuct the filenames for the temporary files and than generate the verilog, exc the external tool, read out
 * the results and convert them back to act.
 *
 * the printing of the verilog is seperate
 */

[[maybe_unused]] ExprBlockInfo *ExternalExprOpt::run_external_opt(const char *expr_set_name, list_t *c_in_exprs,
                                                                  iHashtable *c_in_name_map, iHashtable *c_in_width_map,
                                                                  list_t *c_out_exprs, list_t *c_out_names,
                                                                  iHashtable *c_out_width_map, list_t *c_hidden_exprs,
                                                                  list_t *c_hidden_expr_names) const {

    // widths for the hidden expressions are in the c_out_width_map map


    auto input_name_map = map_val_remap<std::string>(to_map<const Expr *, const char *>(c_in_name_map));
    auto input_width_map = to_imap<const Expr *, int>(c_in_width_map);
    auto input_exprs =to_vector<const Expr *>(c_in_exprs);
    auto leaf_map = zip_mm<NameWithWidth>(input_name_map, input_width_map);


    auto out_width_map = to_imap<const Expr *, int>(c_out_width_map);
    auto out_exprs = zip_vvm<ExprPtrWithNameAndWidth>(to_vector<const Expr *>(c_out_exprs),
                                                      vec_val_remap<std::string>(to_vector<const char *>(c_out_names)),
                                                      out_width_map);
    auto hidden_exprs = zip_vvm<ExprPtrWithNameAndWidth>(
        to_vector<const Expr *>(c_hidden_exprs),
        vec_val_remap<std::string>(to_vector<const char *>(c_hidden_expr_names)), out_width_map);

    return run_external_opt(expr_set_name, input_exprs, leaf_map, out_exprs, hidden_exprs);
}
ExprBlockInfo *ExternalExprOpt::run_external_opt(const std::string &expr_set_name,
                                                 const std::vector<const Expr *> &input_exprs,
                                                 const std::unordered_map<const Expr *, NameWithWidth> &leaf_map,
                                                 const std::vector<ExprPtrWithNameAndWidth> &out_exprs,
                                                 const std::vector<ExprPtrWithNameAndWidth> &hidden_exprs) const {
    ExprBlockInfo *info = nullptr;
    // consruct files names for the temp files
    std::string verilog_file = ".";
    verilog_file.append("/exprop_");
    verilog_file.append(expr_set_name);
    std::string mapped_file = verilog_file;
    verilog_file.append(".v");
    mapped_file.append("_mapped.v");
    char cmd[4096] = "";
    FILE *verilog_stream;

    std::string sdc_file = ".";
    sdc_file.append("/exprop_");
    sdc_file.append(expr_set_name);
    sdc_file.append(".sdc");

    // open temp verilog file to be syntesised

    verilog_stream = fopen(verilog_file.data(), "w");
    if (!verilog_stream)
        fatal_error("ExternalExprOpt::run_external_opt: verilog file %s is not writable", verilog_file.data());

    // generate verilog module
    print_expr_verilog(verilog_stream, expr_set_name, input_exprs, leaf_map, out_exprs, hidden_exprs);

    // force write and close file

    fflush(verilog_stream);
    fclose(verilog_stream);

    // generate the exec command for the sysntesis tool and run the syntesis
    int exec_failure = 1;

#ifdef FOUND_exproptcommercial
    ExprOptCommercialHelper *helper = new ExprOptCommercialHelper();
#endif

    switch (mapper) {
    case ExprMappingSoftware::genus:
#ifdef FOUND_exproptcommercial
        if (expr_output_file.empty())
            exec_failure = helper->run_genus(verilog_file, mapped_file, expr_set_name, true);
        else
            exec_failure = helper->run_genus(verilog_file, mapped_file, expr_set_name);
#else
        fatal_error("cadence genus support was not enabled on compile time");
#endif
        break;

    case ExprMappingSoftware::synopsis:
        // would need a sample script to implement this
        fatal_error("synopsis compiler is not implemented yet");
        break;
    case ExprMappingSoftware::yosys: {
        /* create a .sdc file to get delay values */
        verilog_stream = fopen(sdc_file.data(), "w");
        if (!verilog_stream) {
            fatal_error("Could not open `%s' file!", sdc_file.data());
        }
        fprintf(verilog_stream, "set_load %g\n", config_get_real("expropt.default_load"));
        fclose(verilog_stream);
        // yosys gets its script passed via stdin (very short)
        char *configreturn = config_get_string("expropt.liberty_tt_typtemp");
        if (std::strcmp(configreturn, "none") != 0) {
            int constr = 0;
            if (config_exists("expropt.abc_use_constraints")) {
                if (config_get_int("expropt.abc_use_constraints") == 1) {
                    constr = 1;
                }
            }
            if (use_tie_cells == ShouldTieCells::yes) {
                if (constr) {
                    sprintf(
                        cmd,
                        "echo \"read_verilog %s; synth -noabc -top %s; abc -constr %s -liberty %s; hilomap -hicell "
                        "TIEHIX1 Y -locell TIELOX1 Y -singleton; write_verilog -nohex -nodec %s;\" | yosys > %s.log",
                        verilog_file.data(), expr_set_name.c_str(), sdc_file.data(), configreturn, mapped_file.data(),
                        mapped_file.data());
                } else {
                    sprintf(cmd,
                            "echo \"read_verilog %s; synth -noabc -top %s; abc -liberty %s; hilomap -hicell TIEHIX1 Y "
                            "-locell TIELOX1 Y -singleton; write_verilog -nohex -nodec %s;\" | yosys > %s.log",
                            verilog_file.data(), expr_set_name.c_str(), configreturn, mapped_file.data(),
                            mapped_file.data());
                }
            } else {
                if (constr) {
                    sprintf(cmd,
                            "echo \"read_verilog %s; synth -noabc -top %s; abc -constr %s -liberty %s; write_verilog  "
                            "-nohex -nodec %s;\" | yosys > %s.log",
                            verilog_file.data(), expr_set_name.c_str(), sdc_file.data(), configreturn,
                            mapped_file.data(), mapped_file.data());
                } else {
                    sprintf(cmd,
                            "echo \"read_verilog %s; synth -noabc -top %s; abc -liberty %s; write_verilog  -nohex "
                            "-nodec %s;\" | yosys > %s.log",
                            verilog_file.data(), expr_set_name.c_str(), configreturn, mapped_file.data(),
                            mapped_file.data());
                }
            }
        } else
            fatal_error("please define \"liberty_tt_typtemp\" in expropt configuration file");
        if (config_get_int("expropt.verbose") == 2)
            printf("running: %s \n", cmd);
        else if (config_get_int("expropt.verbose") == 1) {
            printf(".");
            fflush(stdout);
        }
        exec_failure = system(cmd);
        if (exec_failure != 0)
            fatal_error("yosys syntesis failed: \"%s\" failed.", cmd);
        // @TODO do metadata extraction via ABC
        break;
    }
    }

    // read the resulting netlist and map it back to act, if the wire_type is not bool use the async mode the specify a
    // wire type as a channel. skip if run was just for extraction of properties => output filename empty
    if (!expr_output_file.empty()) {
        if (expr_channel_type != "bool")
            sprintf(cmd, "v2act -a -C \"%s\" -l %s -n %s %s >> %s", expr_channel_type.data(), cell_act_file.data(),
                    cell_namespace.data(), mapped_file.data(), expr_output_file.data());
        else
            sprintf(cmd, "v2act -l %s -n %s %s >> %s", cell_act_file.data(), cell_namespace.data(), mapped_file.data(),
                    expr_output_file.data());
        if (config_get_int("expropt.verbose") == 2)
            printf("running: %s \n", cmd);
        else if (config_get_int("expropt.verbose") == 1) {
            printf(".");
            fflush(stdout);
        }
        exec_failure = system(cmd);
        if (exec_failure != 0)
            fatal_error("external program call \"%s\" failed.", cmd);
    }
    // parce block info - WORK IN PROGRESS
    switch (mapper) {
    case ExprMappingSoftware::genus: {
#ifdef FOUND_exproptcommercial
        std::string genus_log = mapped_file.data();
        info = new ExprBlockInfo(helper->parse_genus_log(genus_log, metadata_delay_typ),
                                 helper->parse_genus_log(genus_log, metadata_delay_min),
                                 helper->parse_genus_log(genus_log, metadata_delay_max),
                                 helper->parse_genus_log(genus_log, metadata_power_typ),
                                 helper->parse_genus_log(genus_log, metadata_power_max),
                                 helper->parse_genus_log(genus_log, metadata_area),
                                 helper->parse_genus_log(genus_log, metadata_power_typ_static),
                                 helper->parse_genus_log(genus_log, metadata_power_typ_dynamic),
                                 helper->parse_genus_log(genus_log, metadata_power_max_static),
                                 helper->parse_genus_log(genus_log, metadata_power_max_dynamic));
#else
        fatal_error("cadence genus support was not enabled on compile time");
#endif
    } break;
    case ExprMappingSoftware::synopsis:
        fatal_error("synopsis compiler are not implemented yet");
        break;
    case ExprMappingSoftware::yosys: {
        double delay, area;
        area = 0.0;
        delay = parse_abc_info(mapped_file.data(), &area);
        info = new ExprBlockInfo(delay, 0, 0, 0, 0, 0, 0, 0, 0, area);
    } break;
    }

    // clean up temporary files
    if (cleanup) {
        switch (mapper) {
        case ExprMappingSoftware::genus:
            sprintf(cmd, "rm %s && rm %s && rm %s.* && rm %s.* && rm -r fv* && rm -r rtl_fv* && rm genus.*",
                    mapped_file.data(), verilog_file.data(), mapped_file.data(), verilog_file.data());
            break;
        case ExprMappingSoftware::synopsis:
        case ExprMappingSoftware::yosys:
            sprintf(cmd, "rm %s && rm %s && rm %s && rm %s.* ", mapped_file.data(), verilog_file.data(),
                    sdc_file.data(), mapped_file.data());
            break;
        }
        if (config_get_int("expropt.verbose") == 2)
            printf("running: %s \n", cmd);
        else if (config_get_int("expropt.verbose") == 1) {
            printf(".");
            fflush(stdout);
        }
        exec_failure = system(cmd);
        if (exec_failure != 0)
            warning("external program call \"%s\" failed.", cmd);
    }
    // add exports for namespace support
    // @TODO sed calls are dangorus they change behavior depending on the version installed
    if (!expr_output_file.empty()) {
        sprintf(cmd, "sed -e 's/defproc/export defproc/' -e 's/export export/export/' %s > %sx && mv -f %sx %s",
                expr_output_file.data(), expr_output_file.data(), expr_output_file.data(), expr_output_file.data());
        if (config_get_int("expropt.verbose") == 2)
            printf("running: %s \n", cmd);
        else if (config_get_int("expropt.verbose") == 1) {
            printf(".");
            fflush(stdout);
        }
        exec_failure = system(cmd);
        if (exec_failure != 0)
            fatal_error("external program call \"%s\" failed.", cmd);

        // remove directions on in and outputs to avoid errors on passthrough connections
        sprintf(cmd, "sed -e 's/[\\?!]//g' %s > %sx && mv -f %sx %s", expr_output_file.data(), expr_output_file.data(),
                expr_output_file.data(), expr_output_file.data());

        if (config_get_int("expropt.verbose") == 2)
            printf("running: %s \n", cmd);
        else if (config_get_int("expropt.verbose") == 1) {
            printf(".");
            fflush(stdout);
        }
        exec_failure = system(cmd);
        if (exec_failure != 0)
            fatal_error("external program call \"%s\" failed.", cmd);
    }
    return info;
}

/*
 * work in progress have to swicht to fscanf for float probably.
 */
ExprBlockInfo *ExternalExprOpt::parse_genus_log(const std::string &base_file_name) {
    std::string regex_power(" Subtotal %*f %*f %*f %lf");
    std::string regex_power_dynamic(" Subtotal %*f %*f %lf %*f");
    std::string regex_power_static(" Subtotal %lf %*f %*f %*f");
    std::string regex_power_old("%*s %*f %*f %*f %lf");
    std::string regex_power_dynamic_old("%*s %*f %*f %lf %*f");
    std::string regex_power_static_old("%*s %*f %lf %*f %*f");
    std::string regex_area("%*s %*f %*f %*f %lf");
    std::string regex_delay(" Data Path:- %lf");

    std::string log_name = base_file_name;
    log_name.append(".power_typ.log");
    double power = parse_and_return_max(log_name, regex_power, 0);
    double power_static = parse_and_return_max(log_name, regex_power_static, 0);
    double power_dynamic = parse_and_return_max(log_name, regex_power_dynamic, 0);
    if (power == 0) {
        power = parse_and_return_max(log_name, regex_power_old, 0);
        power_static = parse_and_return_max(log_name, regex_power_static_old, 0);
        power_dynamic = parse_and_return_max(log_name, regex_power_dynamic_old, 0);
        power_static = power_static * 1e-9;
        power_dynamic = power_dynamic * 1e-9;
        power = power * 1e-9;
    }

    log_name = base_file_name;
    log_name.append(".power_max.log");
    double power_max = parse_and_return_max(log_name, regex_power, 0);
    double power_max_static = parse_and_return_max(log_name, regex_power_static, 0);
    double power_max_dynamic = parse_and_return_max(log_name, regex_power_dynamic, 0);
    if (power_max == 0) {
        power_max = parse_and_return_max(log_name, regex_power_old, 0);
        power_max_static = parse_and_return_max(log_name, regex_power_static_old, 0);
        power_max_dynamic = parse_and_return_max(log_name, regex_power_dynamic_old, 0);
        power_max_static = power_max_static * 1e-9;
        power_max_dynamic = power_max_dynamic * 1e-9;
        power_max = power_max * 1e-9;
    }

    // we assume um^2 and we convert to m^2, depends on the liberty file unit
    log_name = base_file_name;
    log_name.append(".area.log");
    double area = parse_and_return_max(log_name, regex_area, 0);
    area = area * 1e-12;

    // we assume ps as set for in genus tcl
    log_name = base_file_name;
    log_name.append(".timing_min.log");
    double delay_min = parse_and_return_max(log_name, regex_delay, 0);
    delay_min = delay_min * 1e-12;

    log_name = base_file_name;
    log_name.append(".timing_max.log");
    double delay_max = parse_and_return_max(log_name, regex_delay, 0);
    delay_max = delay_max * 1e-12;

    log_name = base_file_name;
    log_name.append(".timing_typ.log");
    double delay = parse_and_return_max(log_name, regex_delay, 0);
    delay = delay * 1e-12;

    return new ExprBlockInfo(delay, delay_min, delay_max, power, power_max, area, power_static, power_dynamic,
                             power_max_static, power_max_dynamic);
}

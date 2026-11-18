/*************************************************************************
 *
 *  expr_profile.h -- synthesis profiles for external expression
 *  optimization
 *
 *  A profile is a pair of template files
 *
 *     <profile_dir>/<engine>_<profile>.script synthesis script
 *     <profile_dir>/<engine>_<profile>.sdc    constraints
 *
 *  containing @NAME@ placeholders (NAME made of A-Z, 0-9, _) that are
 *  filled in for every expression block that gets synthesized.
 *
 *  The profile is selected with the configuration string
 *  synth.expropt.profile (synth2 -S <profile>); the template directory
 *  is synth.expropt.profile_dir, by default
 *  $ACT_HOME/conf/$ACT_TECH/expropt_profiles.
 *
 **************************************************************************
 */
#ifndef __ACT_EXPR_PROFILE_H__
#define __ACT_EXPR_PROFILE_H__

#include <string>
#include <map>
#include <set>
#include <vector>

/* Name of the selected profile, or "" if no profile is selected. */
std::string expr_profile_name ();

/* Directory that holds the profile templates. */
std::string expr_profile_dir ();

/* Path of the template <dir>/<engine>_<profile>.<ext> */
std::string expr_profile_template (const std::string &engine,
                                   const std::string &ext);

/* Read a whole file into text; false if it cannot be read. */
bool expr_profile_read_file (const std::string &path, std::string &text);

/*
 * Single pass over text: copy it to out, replacing every placeholder
 * @NAME@ by vars[NAME]. Inserted values are not scanned again.
 * Every placeholder seen is added to *used (if used is not NULL).
 *
 * Returns false if text contains a placeholder that is not in vars;
 * that placeholder is stored in *unknown (if unknown is not NULL).
 */
bool expr_profile_fill (const std::string &text,
                        const std::map<std::string, std::string> &vars,
                        std::string &out,
                        std::set<std::string> *used,
                        std::string *unknown);

/*
 * Instantiate the template <dir>/<engine>_<profile>.<ext> into out_file:
 * fill in vars, and check that every name in required is used and that
 * no unknown placeholder appears.
 *
 * Returns false (after printing a warning) on any problem.
 */
bool expr_profile_instantiate (const std::string &engine,
                               const std::string &ext,
                               const std::map<std::string, std::string> &vars,
                               const std::vector<std::string> &required,
                               const std::string &out_file);

#endif /* __ACT_EXPR_PROFILE_H__ */

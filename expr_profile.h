/* 
 * expr_profile.h -- synthesis profiles for external expression optimization
 * 
 * A profile is a pair of template files
 * 
 *  <profile_dir>/<engine>_<profile>.tcl    synthesis script
 *  <profile_dir>/<engine>_<profile>.sdc    constraints
 * 
 * containing @NAME@ placeholders that are filled in for every expression that gets synthesized.
 * 
 * header only on purpose: the act_extsyn_*.so plugins link only against libact, so everything here is defined inline.
 *
 */

#ifndef __ACT_EXPR_PROFILE_H__
#define __ACT_EXPR_PROFILE_H__

#include <string>
#include <vector>
#include <map>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <common/config.h>
#include <common/misc.h>


/* 
 * replace every @KEY@ in the text by vars[KEY].
 * placeholders without an entry in vars are left untouched
 */

static inline std::string
expr_profile_fill ( const std::string &text,
                    const std::map<std::string, std::string> &vars )
{
    std::string out = text;
    for (const auto &kv : vars){
        const std::string key = '@' + kv.first + '@';
        size_t pos =0;
        while ((pos=out.find (key,pos)) != std::string::npos){
            out.replace (pos,key.size(),kv.second);
            pos += kv.second.size();
        }
    }
    return out;
}


/*
 * return the first placeholder @NAME@ (NAME made of A-Z, 0-9, _)
 * still present in text, or "" if there is none
 */

static inline std::string
expr_profile_leftover (const std::string &text)
{
    size_t pos=0;
    while ((pos = text.find ('@', pos)) != std::string::npos) {
    size_t end = pos+1;
    while (end<text.size() 
            && (isupper (text[end]) || isdigit(text[end]) || text[end] == '_' )) { // read for all upper digit and _
        end++;
    }
        if(end> pos+1 && end<text.size() && text[end]=='@'){  //is placeholder if long one character and end with @
            return text.substr (pos,end-pos+1);
        }
        pos=end;
    }
    return "";
}


/*
 * return the first name in required whose @NAME@ does not appear
 * in text, or "" if all of them are present
 */

static inline std::string
expr_profile_missing (const std::string &text,
                      const std::vector<std::string> &required)
{
    for (const auto &name :required){
        if(text.find ("@"+name+"@")==std::string::npos) {
            return name;
        }
    }
    return "";
}


/*------------------------------------------------------------------------
 *  Files and configuration
 *------------------------------------------------------------------------
 */

/*
 * Name of the selected profile (config: synth.expropt.profile),
 * or "" if no profile is selected.
 */
static inline std::string
expr_profile_name ()
{
  if (!config_exists ("synth.expropt.profile")) {
    return "";
  }
  return config_get_string ("synth.expropt.profile");
}


/*
 * Directory holding the templates (config: synth.expropt.profile_dir).
 * Default: $ACT_HOME/conf/$ACT_TECH/expropt_profiles
 */
static inline std::string
expr_profile_dir ()
{
  if (config_exists ("synth.expropt.profile_dir")) {
    return config_get_string ("synth.expropt.profile_dir");
  }
  const char *home = getenv ("ACT_HOME");
  const char *tech = getenv ("ACT_TECH");
  return std::string (home ? home : ".") + "/conf/" +
    (tech ? tech : "generic") + "/expropt_profiles";
}

/*
 * Full path of the template <dir>/<engine>_<profile>.<ext>
 */
static inline std::string
expr_profile_template (const std::string &engine, const std::string &ext)
{
  return expr_profile_dir () + "/" + engine + "_" + expr_profile_name () +
    "." + ext;
}


/* Read a whole file into text. */
static inline bool
expr_profile_read_file (const std::string &path, std::string &text)
{
  std::ifstream in (path);
  if (!in) {
    return false;
  }
  std::stringstream ss;
  ss << in.rdbuf ();
  text = ss.str ();
  return true;
}

/* Write text to a file, replacing it. */
static inline bool
expr_profile_write_file (const std::string &path, const std::string &text)
{
  std::ofstream out (path);
  if (!out) {
    return false;
  }
  out << text;
  return out.good ();
}


/*
 * Instantiate the template <dir>/<engine>_<profile>.<ext> into out_file:
 * check that every name in required appears as @NAME@, fill in vars,
 * and refuse to write a script that still contains a placeholder.
 *
 * Returns false (after printing a warning) on any problem.
 */

static inline bool
expr_profile_instantiate (  const std::string &engine,
                            const std::string &ext,
                            const std::map<std::string,std::string> &vars,
                            const std::vector<std::string> &required,
                            const std::string &out_file){
    
    std::string path=expr_profile_template (engine,ext);
    std::string text;
    
    if(!expr_profile_read_file (path,text)){
        warning("synthesis profile : '%s': cannot read template '%s'",
                expr_profile_name ().c_str(),path.c_str());
    return false;
    }

    std::string missing = expr_profile_missing (text,required);
    if (!missing.empty ()){
        warning("template %s does not use required placeholder @%s@",
                path.c_str(), missing.c_str());
        return false;
    }

    text = expr_profile_fill (text,vars);

    std::string left = expr_profile_leftover (text);
    if (!left.empty ()){
        warning ("template %s uses unknown placeholder %s",path.c_str(), left.c_str());
        return false;
    }

    if(!expr_profile_write_file(out_file,text)){
        warning("cannot write %s",out_file.c_str());
        return false;
    }
    return true;
}



#endif /* __ACT_EXPR_PROFILE_H__ */




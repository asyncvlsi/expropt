/*************************************************************************
 *
 *  expr_profile.cc -- synthesis profiles for external expression
 *  optimization (see expr_profile.h)
 *
 **************************************************************************
 */
#include "expr_profile.h"
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <common/config.h>
#include <common/misc.h>

std::string expr_profile_name ()
{
  if (!config_exists ("synth.expropt.profile")) {
    return "";
  }
  return config_get_string ("synth.expropt.profile");
}

std::string expr_profile_dir ()
{
  if (config_exists ("synth.expropt.profile_dir")) {
    return config_get_string ("synth.expropt.profile_dir");
  }
  const char *home = getenv ("ACT_HOME");
  const char *tech = getenv ("ACT_TECH");
  return std::string (home ? home : ".") + "/conf/" +
    (tech ? tech : "generic") + "/expropt_profiles";
}

std::string expr_profile_template (const std::string &engine,
                                   const std::string &ext)
{
  return expr_profile_dir () + "/" + engine + "_" + expr_profile_name () +
    "." + ext;
}


bool expr_profile_read_file (const std::string &path, std::string &text)
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

static bool write_file (const std::string &path, const std::string &text)
{
  std::ofstream out (path);
  if (!out) {
    return false;
  }
  out << text;
  return out.good ();
}

static bool is_name_char (char c)
{
  return isupper ((unsigned char) c) || isdigit ((unsigned char) c) || c == '_';
}

bool expr_profile_fill (const std::string &text,
                        const std::map<std::string, std::string> &vars,
                        std::string &out,
                        std::set<std::string> *used,
                        std::string *unknown)
{
  out.clear ();
  out.reserve (text.size ());

  size_t pos = 0;
  while (pos < text.size ()) {
    size_t at = text.find ('@', pos);
    if (at == std::string::npos) {
      out.append (text, pos, std::string::npos);
      break;
    }
    /* copy everything up to the '@' */
    out.append (text, pos, at - pos);

    /* is this the start of a placeholder @NAME@ ? */
    size_t end = at + 1;
    while (end < text.size () && is_name_char (text[end])) {
      end++;
    }
    if (end > at + 1 && end < text.size () && text[end] == '@') {
      std::string name = text.substr (at + 1, end - at - 1);
      std::map<std::string, std::string>::const_iterator it = vars.find (name);
      if (it == vars.end ()) {
        if (unknown) {
          *unknown = "@" + name + "@";
        }
        return false;
      }
      out += it->second;
      if (used) {
        used->insert (name);
      }
      pos = end + 1;
    }
    else {
      /* a lone '@': copy it and continue right after it */
      out += '@';
      pos = at + 1;
    }
  }
  return true;
}



bool expr_profile_instantiate (const std::string &engine,
                               const std::string &ext,
                               const std::map<std::string, std::string> &vars,
                               const std::vector<std::string> &required,
                               const std::string &out_file)
{
  std::string path = expr_profile_template (engine, ext);
  std::string text;

  if (!expr_profile_read_file (path, text)) {
    warning ("synthesis profile `%s': cannot read template `%s'",
             expr_profile_name ().c_str(), path.c_str());
    return false;
  }

  std::string filled, unknown;
  std::set<std::string> used;
  if (!expr_profile_fill (text, vars, filled, &used, &unknown)) {
    warning ("template `%s' uses unknown placeholder %s",
             path.c_str(), unknown.c_str());
    return false;
  }

  for (size_t i = 0; i < required.size (); i++) {
    if (used.find (required[i]) == used.end ()) {
      warning ("template `%s' does not use required placeholder @%s@",
               path.c_str(), required[i].c_str());
      return false;
    }
  }

  if (!write_file (out_file, filled)) {
    warning ("cannot write `%s'", out_file.c_str());
    return false;
  }
  return true;
}

/*************************************************************************
 *
 *  Copyright (c) 2023 Rajit Manohar
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
#ifndef __EXPROPT_ABC_API_H__
#define __EXPROPT_ABC_API_H__

/*
 * Minimal API to abc
 */
extern "C" {

  void Abc_Start ();
  void Abc_Stop ();
  typedef struct Abc_Frame_t_ Abc_Frame_t;
  Abc_Frame_t *Abc_FrameGetGlobalFrame();
  int Cmd_CommandExecute (Abc_Frame_t *pAbc, const char *sCommand);

}

class AbcApi {
public:
  AbcApi ();
  ~AbcApi ();

  /*
   * @param v_in is the input Verilog file
   * @param v_out is where the mapped Verilog netlist should be saved
   * @param name is the name of the top-level module
   */
  int startSession (const char *v_in, const char *v_out, const char *name);
  
  int runCmd (const char *cmd);
  int stdSynthesis ();
  int runTiming ();
  int endSession ();

 private:
  char *_name;			// name of the module
  char *_vin;			// Verilog input file
  char *_vout;			// Verilog output file
  
  bool _parent;
  int _childpid;
  Abc_Frame_t *_pAbc;

  int _logfd;

  struct {
    int from;   // inbound file descriptor
    int to;	// outbound file descriptor
  } _fd;


  void _mainloop ();

  int _get_full_cmd (char **buf, int *maxsz, int *sz, int *pos);
  
  void _send (const char *s);
  void _recv (char *buf, int sz);

  bool _startsession (char *name);
  bool _endsession ();

  bool _run_abc (char *cmd);

  // reply to parent
  void _ok ();
  void _error ();

  // check in parent
  int _check_ok ();
};
    


#endif /* __EXPROPT_ABC_API_H__ */

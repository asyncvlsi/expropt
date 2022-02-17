/*************************************************************************
 *
 *  Copyright (c) 2021 Rajit Manohar
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
#include "chpexprexample.h"
#include <act/lang/act.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

static void usage(char *name) {
    fprintf(stderr, "Usage: %s -o[<yosys,genus>] <actfile> <process> <out>\n", name);
    exit(1);
}

int main(int argc, char **argv) {
    Act *a;
    //  char *proc;
    [[maybe_unused]] char *syntesistool = NULL;

    /* initialize ACT library */
    Act::Init(&argc, &argv);

    int ch;
    while ((ch = getopt(argc, argv, "o:")) != -1) {
        switch (ch) {
        case 'o':
            syntesistool = Strdup(optarg);
            break;
        default:
            usage(argv[0]);
            break;
        }
    }

    if (optind != argc - 3) {
        usage(argv[0]);
    }

    /* read in the ACT file */
    a = new Act(argv[optind]);

    /* expand it */
    a->Expand();

    /* find the process specified on the command line */
    Process *p = a->findProcess(argv[optind + 1]);

    if (!p) {
        fatal_error("Could not find process `%s' in file `%s'", argv[optind + 1], argv[optind]);
    }

    if (!p->isExpanded()) {
        // fatal_error("Process `%s' is not expanded.", argv[optind+1]);
        p = p->Expand(ActNamespace::Global(), p->CurScope(), 0, NULL);
    }
    Assert(p, "What?");

    /* extract the chp */
    if (p->getlang() == NULL || p->getlang()->getchp() == NULL) {
        fatal_error("Process `%s' does not have any chp.", argv[optind + 1]);
    }

    chpexprexample *chp2v = new chpexprexample(argv[optind + 2], yosys, a, p);
    chp2v->process_chp();
    // std::string log_file_name = "exprop_set2.v";
    // chp2v->optimiser->parse_genus_log(log_file_name);
    return 0;
}

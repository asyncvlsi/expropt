#-------------------------------------------------------------------------
#
#  Copyright (c) 2021 Rajit Manohar
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor,
#  Boston, MA  02110-1301, USA.
#
#-------------------------------------------------------------------------

LIB=libexpropt_$(EXT).a
SHLIB=libexpropt_sh_$(EXT).so

include config.mk

TARGETLIBS=$(LIB) $(SHLIB)

TARGETINCS=expropt.h

TARGETINCSUBDIR=act

TARGETCONF=expropt.conf

OBJS2=expropt.o abc_api.o

OBJS= $(OBJS2)

ifdef exproptcommercial_INCLUDE
EXPRCOMLIB=-lexproptcommercial_sh
else
EXPRCOMLIB=
endif

ifdef FOUND_READLINE
RLIBS=$(READLINELIB) -lreadline
else
RLIBS=
endif

ifdef abc_LIBDIR
RLIBS := $(abc_LIBDIR) -labc $(RLIBS)
else
RLIBS=
endif

SHOBJS=$(OBJS:.o=.os)

SRCS= $(OBJS2:.o=.cc)

#SUBDIRSPOST=test

include $(ACT_HOME)/scripts/Makefile.std

$(LIB): $(OBJS) 
	ar ruv $(LIB) $(OBJS)
	$(RANLIB) $(LIB)

$(SHLIB): $(SHOBJS) 
	$(ACT_HOME)/scripts/linkso $(SHLIB) $(SHOBJS) $(SHLIBACT) $(EXPRCOMLIB) $(RLIBS)

SUBDIRS=example

debug:
	@if [ -d $(EXT) -a -f $(EXT)/expropt.o ] ; \
	then \
		(mv $(EXT)/expropt.o expropt.o); \
	fi

-include Makefile.deps

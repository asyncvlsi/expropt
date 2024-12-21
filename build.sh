#!/bin/sh

git submodule update --init

echo "Building abc: static library..."
(cd abc; make -j 4 ABC_USE_NO_READLINE=1 libabc.a; cp libabc.a $ACT_HOME/lib)

echo "Building abc: dynamic library..."
(cd abc2; make -j 4 ABC_USE_NO_READLINE=1 ABC_USE_PIC=1 libabc.so; cp libabc.so $ACT_HOME/lib)

echo "Building expropt..."
make "$@" depend && make "$@" && make install

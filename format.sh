#!/usr/bin/bash

clang-format -style=file -i expropt.cc expropt.h
cmake-format -i CMakeLists.txt

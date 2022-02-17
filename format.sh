#!/usr/bin/bash

find src example -name "*.cc" -or -name "*.h" | xargs clang-format -style=file -i
find -name CMakeLists.txt | xargs cmake-format -i

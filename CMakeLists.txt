cmake_minimum_required(VERSION 3.20)

project(ExprOpt)
enable_language(CXX)
enable_testing()

# ensure the environment is configured correctly. We were doing shared libraries
# wrong before. The right way is to have their location added to LD_LIBRARY_PATH
if(NOT DEFINED ENV{ACT_HOME})
  message(
    FATAL_ERROR "Set the enviroment variable 'ACT_HOME' to the install location"
  )
endif()
string(REGEX MATCH "^(.*:)?$ENV{ACT_HOME}/lib/?(:.*)?$"
             ACTHOME_LIB_IS_IN_LD_PATH $ENV{LD_LIBRARY_PATH})
if(NOT ACTHOME_LIB_IS_IN_LD_PATH)
  message(FATAL_ERROR "Add $ENV{ACT_HOME}/lib to LD_LIBRARY_PATH")
endif()

#
include_directories($ENV{ACT_HOME}/include/)
add_library(ActLang STATIC IMPORTED)
set_target_properties(ActLang PROPERTIES IMPORTED_LOCATION
                                         $ENV{ACT_HOME}/lib/libact.a)
add_library(ActCommon STATIC IMPORTED)
set_target_properties(ActCommon PROPERTIES IMPORTED_LOCATION
                                           $ENV{ACT_HOME}/lib/libvlsilib.a)

add_library(ActLangSh SHARED IMPORTED)
set_target_properties(ActLangSh PROPERTIES IMPORTED_LOCATION
                                           $ENV{ACT_HOME}/lib/libact_sh.so)
add_library(ActCommonSh SHARED IMPORTED)
set_target_properties(
  ActCommonSh PROPERTIES IMPORTED_LOCATION $ENV{ACT_HOME}/lib/libvlsilib_sh.so)

set(EXPECTED_EXPROPTCOMM_PATH $ENV{ACT_HOME}/lib/exproptcommercial_sh.so)
if(EXISTS ${EXPECTED_EXPROPTCOMM_PATH})
  message("Found Library exproptcomm: YES")
  add_library(ActExprOptCommShared SHARED IMPORTED)
  set_target_properties(
    ActExprOptCommShared PROPERTIES IMPORTED_LOCATION
                                    ${EXPECTED_EXPROPTCOMM_PATH})
  add_compile_definitions(FOUND_exproptcommercial)
else()
  message("Found Library exproptcomm: NO")
endif()

# hack to get around old configuration method
add_compile_definitions(__CONFIG_PKG_H__)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wcast-qual -Wconversion-null
                    -Wmissing-declarations -Woverlength-strings)
add_compile_options(-Wpointer-arith -Wunused-local-typedefs -Wunused-result
                    -Wvarargs -Wvla -Wwrite-strings)
add_compile_options(-Wformat-security -Wundef)
add_compile_options(-g -fsanitize=undefined -Werror)
add_link_options(-fsanitize=undefined)
# set(CMAKE_CXX_CLANG_TIDY clang-tidy; -checks=*; -warnings-as-errors=*;)

add_subdirectory(src)
add_subdirectory(example)

install(FILES expropt.conf DESTINATION $ENV{ACT_HOME}/conf/generic/)

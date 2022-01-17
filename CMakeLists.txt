cmake_minimum_required(VERSION 3.20)

project(ChpOptimizer)
enable_language(CXX)
enable_testing()
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
add_link_options(-ldl -fsanitize=undefined)
# set(CMAKE_CXX_CLANG_TIDY clang-tidy; -checks=*; -warnings-as-errors=*;)

# This is just for testing. Doesnt do a real build/install
add_library(ExprOptLib STATIC expropt.h expropt.cc)
add_library(ExprOptLibSh SHARED expropt.h expropt.cc)

if(EXISTS ${EXPECTED_EXPROPTCOMM_PATH})
  target_link_libraries(ExprOptLib ActLang ActCommon ActExprOptCommShared)
  target_link_libraries(ExprOptLibSh ActLangSh ActCommonSh ActExprOptCommShared)
else()
  target_link_libraries(ExprOptLib ActLang ActCommon)
  target_link_libraries(ExprOptLibSh ActLangSh ActCommonSh)
endif()

set_target_properties(ExprOptLib PROPERTIES OUTPUT_NAME "libexpropt")
set_target_properties(ExprOptLibSh PROPERTIES OUTPUT_NAME "libexpropt_sh")

install(FILES expropt.conf DESTINATION $ENV{ACT_HOME}/conf/generic/)
install(FILES expropt.h DESTINATION $ENV{ACT_HOME}/include/act/)
install(TARGETS ExprOptLib ExprOptLibSh DESTINATION $ENV{ACT_HOME}/bin/)

add_library(ExprOptLib STATIC expropt.h expropt.cc)
add_library(ExprOptLibSh SHARED expropt.h expropt.cc)

if(EXISTS ${EXPECTED_EXPROPTCOMM_PATH})
  target_link_libraries(ExprOptLib ActLang ActCommon ActExprOptCommShared)
  target_link_libraries(ExprOptLibSh ActLangSh ActCommonSh ActExprOptCommShared)
else()
  target_link_libraries(ExprOptLib ActLang ActCommon)
  target_link_libraries(ExprOptLibSh ActLangSh ActCommonSh)
endif()

set_target_properties(ExprOptLib PROPERTIES OUTPUT_NAME "expropt")
set_target_properties(ExprOptLibSh PROPERTIES OUTPUT_NAME "expropt_sh")

install(FILES expropt.h DESTINATION $ENV{ACT_HOME}/include/act/expropt/)
install(
  FILES expropt.h.shim
  DESTINATION $ENV{ACT_HOME}/include/act/
  RENAME expropt.h)
install(TARGETS ExprOptLib ExprOptLibSh DESTINATION $ENV{ACT_HOME}/lib/)

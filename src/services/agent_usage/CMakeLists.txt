# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(qindaqt_agent_usage STATIC)
add_library(QindaQt::AgentUsage ALIAS qindaqt_agent_usage)
target_sources(qindaqt_agent_usage PRIVATE src/report_parser.cpp src/codex_parser.cpp src/agent_usage_collector.cpp
 PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
 include/qindaqt/services/agent_usage/agent_usage_types.h
 include/qindaqt/services/agent_usage/agent_usage_source.h
 include/qindaqt/services/agent_usage/agent_usage_collector.h)
target_include_directories(qindaqt_agent_usage PRIVATE src)
target_link_libraries(qindaqt_agent_usage PUBLIC Qt6::Core)
target_compile_features(qindaqt_agent_usage PUBLIC cxx_std_20)
set_target_properties(qindaqt_agent_usage PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_agent_usage)
install(TARGETS qindaqt_agent_usage EXPORT QindaQtTargets FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(PROGRAMS "${PROJECT_SOURCE_DIR}/tools/qindaqt-agent-usage-report" DESTINATION "${CMAKE_INSTALL_BINDIR}")

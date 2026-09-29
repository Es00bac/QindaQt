# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-CONTRACT: QindaQtKWinDecoration (qindaqt-kwin, ADR-0291) publishes the relative plugin directory used by
# both the production install rule and this staged-install contract check.
# Only builds with the native plugin need it; bridge-only lanes (CI on a
# distribution KWin) have no qindaqt-kwin and never use the plugin directory.
if(QINDAQT_BUILD_KWIN_PLUGIN)
    find_package(QindaQtKWinDecoration 6.6.6.1 EXACT REQUIRED CONFIG)
endif()

foreach(test_name IN ITEMS compositorconfigimport installpaths kwincommandbuilder sessioncommandline sessionbusbootstrap sessiondefaults sessionenvironment)
    add_executable(tst_${test_name} tst_${test_name}.cpp)
    target_compile_features(tst_${test_name} PRIVATE cxx_std_20)
    target_link_libraries(tst_${test_name} PRIVATE QindaQt::SessionSupport Qt6::Test)
    set_target_properties(tst_${test_name} PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(tst_${test_name})
    add_test(NAME session.${test_name} COMMAND tst_${test_name})
endforeach()
target_link_libraries(tst_sessiondefaults PRIVATE KF6::ConfigCore)


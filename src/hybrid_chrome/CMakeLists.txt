# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Widgets)

qt_add_library(qindaqt_hybrid_chrome STATIC)
add_library(QindaQt::HybridChrome ALIAS qindaqt_hybrid_chrome)

target_sources(
    qindaqt_hybrid_chrome
    PRIVATE
        src/chromehittest.cpp
        src/chromelayoutengine.cpp
        src/chromerenderer.cpp
        src/chrometypes.cpp
        src/chromewidget.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/hybrid_chrome/chromehittest.h
            include/qindaqt/hybrid_chrome/chromelayoutengine.h
            include/qindaqt/hybrid_chrome/chromerenderer.h
            include/qindaqt/hybrid_chrome/chrometypes.h
            include/qindaqt/hybrid_chrome/chromewidget.h
)

target_compile_features(qindaqt_hybrid_chrome PUBLIC cxx_std_20)
target_link_libraries(qindaqt_hybrid_chrome PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
set_target_properties(
    qindaqt_hybrid_chrome
    PROPERTIES
        CXX_EXTENSIONS OFF
        # This presentation-neutral archive is linked into KWin's module.
        POSITION_INDEPENDENT_CODE ON
)
if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_hybrid_chrome)
endif()

install(
    TARGETS qindaqt_hybrid_chrome
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

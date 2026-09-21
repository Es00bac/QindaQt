# SPDX-License-Identifier: GPL-3.0-or-later

# Pure geometry for the gather overview. No KWin, no Qt GUI, no global state:
# the planner takes a work area and three inventories and returns rectangles.
qt_add_library(qindaqt_hybrid_gather STATIC)
add_library(QindaQt::HybridGather ALIAS qindaqt_hybrid_gather)

target_sources(
    qindaqt_hybrid_gather
    PRIVATE
        src/gather_layout.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/hybrid_gather/gather_layout.h
)

target_link_libraries(qindaqt_hybrid_gather PUBLIC Qt6::Core)
target_compile_features(qindaqt_hybrid_gather PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_hybrid_gather
    PROPERTIES
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
)

if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_hybrid_gather)
endif()

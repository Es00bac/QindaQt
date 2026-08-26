# SPDX-License-Identifier: LGPL-3.0-or-later

add_library(qindaqt_hybrid STATIC)
target_sources(
    qindaqt_hybrid
    PRIVATE
        topologycommand.cpp
        topologycoordinator.cpp
        topologyplacementmutation.cpp
        topologymutation.cpp
        topologymutation_p.h
        windowtopology.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/hybrid/topologycommand.h
            include/qindaqt/hybrid/topologycoordinator.h
            include/qindaqt/hybrid/topologyscene.h
            include/qindaqt/hybrid/windowtopology.h
)
add_library(QindaQt::Hybrid ALIAS qindaqt_hybrid)

target_compile_features(qindaqt_hybrid PUBLIC cxx_std_20)
target_link_libraries(qindaqt_hybrid PUBLIC QindaQt::Core Qt6::Core)

set_target_properties(qindaqt_hybrid PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_hybrid)
endif()

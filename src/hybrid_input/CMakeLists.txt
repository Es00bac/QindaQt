# SPDX-License-Identifier: GPL-3.0-or-later

add_library(qindaqt_hybrid_input STATIC)
add_library(QindaQt::HybridInput ALIAS qindaqt_hybrid_input)

target_sources(
    qindaqt_hybrid_input
    PRIVATE
        src/interactioncontroller.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/hybrid_input/interactioncontroller.h
            include/qindaqt/hybrid_input/interactiontargetresolver.h
            include/qindaqt/hybrid_input/interactiontypes.h
)

target_compile_features(qindaqt_hybrid_input PUBLIC cxx_std_20)
target_link_libraries(qindaqt_hybrid_input PUBLIC Qt6::Core)
set_target_properties(
    qindaqt_hybrid_input
    PROPERTIES
        CXX_EXTENSIONS OFF
        # The production input adapter and compositor plugin consume this
        # static policy library from shared-object code.
        POSITION_INDEPENDENT_CODE ON
)

if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_hybrid_input)
endif()

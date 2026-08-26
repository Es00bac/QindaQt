# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_hybrid_constraints STATIC)
add_library(QindaQt::HybridConstraints ALIAS qindaqt_hybrid_constraints)

target_sources(
    qindaqt_hybrid_constraints
    PRIVATE
        src/constraint_solver.cpp
        src/member_size_constraints.cpp
        src/window_restore_state.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/hybrid_constraints/constraint_solution.h
            include/qindaqt/hybrid_constraints/constraint_solver.h
            include/qindaqt/hybrid_constraints/member_size_constraints.h
            include/qindaqt/hybrid_constraints/window_restore_state.h
)

target_link_libraries(qindaqt_hybrid_constraints PUBLIC QindaQt::Core Qt6::Core)
target_compile_features(qindaqt_hybrid_constraints PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_hybrid_constraints
    PROPERTIES
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
)

if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_hybrid_constraints)
endif()

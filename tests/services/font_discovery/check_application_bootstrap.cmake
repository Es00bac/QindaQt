# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

# AGENT-NOTE: Review finding P1-6 of rejected candidate abc76f3 placed a large
# bootstrap block after QGuiApplication construction in every first-party app.
# Keep policy inside FontSessionBootstrap and require exactly one call before
# construction in each composition root; the runtime child row separately
# proves that the helper itself is fail-closed and uses production discovery.
# ADR-0116 re-scoped the call set: the stock-Qt6 applications (Terminal, Text
# Editor) take fonts from the Qt platform theme and must NOT call the
# bootstrap; the QST-consuming applications keep exactly one guarded call.
set(application_sources
    "src/apps/calendar/main.cpp"
    "src/apps/file_manager/main.cpp"
    "src/apps/settings_center/main.cpp"
)
foreach(relative_path IN LISTS application_sources)
    set(source_path "${SOURCE_ROOT}/${relative_path}")
    file(READ "${source_path}" content)

    string(REGEX MATCHALL
        "QindaQt::Services::FontDiscovery::FontSessionBootstrap::[ \t\r\n]*applyFromSessionSettings\\(\\)"
        calls "${content}"
    )
    list(LENGTH calls call_count)
    if(NOT call_count EQUAL 1)
        message(FATAL_ERROR
            "${relative_path} must contain exactly one guarded FontSessionBootstrap call; found ${call_count}"
        )
    endif()

    list(GET calls 0 call_text)
    string(FIND "${content}" "${call_text}" call_position)
    string(REGEX MATCH "Q(Gui)?Application application\\(argc, argv\\);" construction "${content}")
    if(construction STREQUAL "")
        message(FATAL_ERROR "${relative_path} has no recognized application construction")
    endif()
    string(FIND "${content}" "${construction}" construction_position)
    if(call_position LESS 0 OR construction_position LESS 0
       OR NOT call_position LESS construction_position)
        message(FATAL_ERROR
            "${relative_path} must call FontSessionBootstrap before application construction"
        )
    endif()
endforeach()

# ADR-0116: the stock-Qt6 widget applications must not carry the bootstrap at
# all; their fonts arrive through the Qt platform theme.
set(stock_qt6_application_sources
    "src/apps/terminal/main.cpp"
    "src/apps/text_editor/main.cpp"
)
foreach(relative_path IN LISTS stock_qt6_application_sources)
    set(source_path "${SOURCE_ROOT}/${relative_path}")
    file(READ "${source_path}" content)

    string(REGEX MATCHALL
        "QindaQt::Services::FontDiscovery::FontSessionBootstrap::[ \t\r\n]*applyFromSessionSettings\\(\\)"
        calls "${content}"
    )
    list(LENGTH calls call_count)
    if(NOT call_count EQUAL 0)
        message(FATAL_ERROR
            "${relative_path} must not call FontSessionBootstrap (ADR-0116 stock Qt 6 app); found ${call_count}"
        )
    endif()
endforeach()

message(STATUS "All first-party applications have one pre-application FontSessionBootstrap call")

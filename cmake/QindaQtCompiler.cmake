# SPDX-License-Identifier: LGPL-3.0-or-later

function(qindaqt_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(QINDAQT_ENABLE_STRICT_WARNINGS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
        return()
    endif()

    target_compile_options(
        ${target}
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
    )

    if(QINDAQT_ENABLE_STRICT_WARNINGS)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()

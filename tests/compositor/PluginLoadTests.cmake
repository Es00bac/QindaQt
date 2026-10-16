# SPDX-License-Identifier: GPL-3.0-or-later
#
# The built plugin must load against the installed fork with every symbol
# resolved. A reference to a fork class whose metaobject or typeinfo the fork
# does not export (qobject_cast<KWin::WaylandWindow>, found 2026-10-16) links
# cleanly and then makes KWin refuse the whole plugin at startup, which no
# unit row notices.
if(TARGET qindaqt_compositor)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(
        NAME compositor.kwin-plugin-symbols-resolve
        COMMAND
            "${Python3_EXECUTABLE}" -c
            "import ctypes, os, sys; ctypes.CDLL(sys.argv[1], mode=os.RTLD_NOW | os.RTLD_LOCAL)"
            "$<TARGET_FILE:qindaqt_compositor>"
    )
    set_tests_properties(
        compositor.kwin-plugin-symbols-resolve
        PROPERTIES LABELS "compositor;packaging" TIMEOUT 30
    )
endif()

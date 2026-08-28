file(READ "${DESKTOP_FILE}" desktop)
foreach(required
        "Type=Application"
        "Name=QindaQt Terminal"
        "GenericName=Terminal Emulator"
        "Exec=qindaqt-terminal"
        "Icon=utilities-terminal"
        "Categories=Qt;System;TerminalEmulator;"
        "Terminal=false"
        "StartupWMClass=qindaqt-terminal")
    string(FIND "${desktop}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "desktop metadata is missing: ${required}")
    endif()
endforeach()
# A terminal must never declare itself a Terminal= application and must not
# advertise a MimeType it does not open.
foreach(forbidden
        "MimeType="
        "Terminal=true")
    string(FIND "${desktop}" "${forbidden}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "desktop metadata must not contain: ${forbidden}")
    endif()
endforeach()

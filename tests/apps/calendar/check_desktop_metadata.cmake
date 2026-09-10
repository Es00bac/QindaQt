# SPDX-License-Identifier: GPL-3.0-or-later
file(READ "${DESKTOP_FILE}" desktop)
foreach(required
        "Type=Application"
        "Name=QindaQt Calendar"
        "Exec=qindaqt-calendar %u"
        "Icon=org.qindaqt.Calendar"
        "Categories=Office;Calendar;"
        "MimeType=text/calendar;"
        "StartupWMClass=qindaqt-calendar"
        "Terminal=false")
    string(FIND "${desktop}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "desktop metadata is missing: ${required}")
    endif()
endforeach()

file(READ "${DESKTOP_FILE}" desktop)
foreach(required
        "Type=Application"
        "Name=QindaQt Text Editor"
        "Exec=qindaqt-editor %f"
        "Categories=Qt;Utility;TextEditor;"
        "MimeType=text/plain;"
        "Terminal=false")
    string(FIND "${desktop}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "desktop metadata is missing: ${required}")
    endif()
endforeach()

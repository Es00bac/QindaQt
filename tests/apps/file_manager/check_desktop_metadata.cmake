file(READ "${DESKTOP_FILE}" desktop)
foreach(required
        "Type=Application"
        "Name=QindaQt File Manager"
        "Exec=qindaqt-file-manager %u"
        "Categories=Qt;System;FileTools;FileManager;"
        "MimeType=inode/directory;"
        "Terminal=false")
    string(FIND "${desktop}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "desktop metadata is missing: ${required}")
    endif()
endforeach()

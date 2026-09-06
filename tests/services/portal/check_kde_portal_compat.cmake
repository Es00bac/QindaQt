foreach(required IN ITEMS DROPIN_FILE KDE_DBUS_SERVICE KDE_PORTAL_METADATA)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing KDE portal compatibility input: ${required}")
    endif()
endforeach()

foreach(path IN ITEMS DROPIN_FILE KDE_DBUS_SERVICE KDE_PORTAL_METADATA)
    if(NOT EXISTS "${${path}}")
        message(FATAL_ERROR "Missing KDE portal compatibility file: ${${path}}")
    endif()
endforeach()

file(READ "${DROPIN_FILE}" dropin)
file(READ "${KDE_DBUS_SERVICE}" dbus_service)
file(READ "${KDE_PORTAL_METADATA}" portal_metadata)

if(NOT dropin MATCHES "\\[Service\\]"
   OR NOT dropin MATCHES "Environment=XDG_CURRENT_DESKTOP=KDE")
    message(FATAL_ERROR
        "KDE portal compatibility drop-in must scope XDG_CURRENT_DESKTOP=KDE")
endif()
if(NOT dbus_service MATCHES
       "SystemdService=plasma-xdg-desktop-portal-kde\\.service")
    message(FATAL_ERROR
        "KDE D-Bus activation does not delegate to the drop-in-covered systemd unit")
endif()
if(NOT portal_metadata MATCHES
       "org\\.freedesktop\\.impl\\.portal\\.RemoteDesktop")
    message(FATAL_ERROR
        "KDE portal metadata no longer advertises RemoteDesktop")
endif()
if(dropin MATCHES "XDG_CURRENT_DESKTOP=QindaQt"
   OR dropin MATCHES "XDG_CURRENT_DESKTOP=QINDAQT")
    message(FATAL_ERROR "KDE compatibility drop-in must not change QindaQt's identity")
endif()

message(STATUS "KDE portal compatibility activation contract is valid")

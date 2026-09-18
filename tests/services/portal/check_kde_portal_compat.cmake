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

# ADR-0170: the identity above only matters if it reaches the process that ends
# up owning the bus name. On a session running the private bus QindaQt
# bootstraps itself, the systemd user manager cannot observe a Type=dbus unit
# taking its name, terminates the working backend at TimeoutStartSec, and D-Bus
# activation respawns it without this drop-in - registering no ScreenCast at
# all. Judging the start by exec success is observable on any bus.
# Anchored to a line of its own: the explanation above mentions Type=exec in
# prose, and a contract test that its own comment satisfies proves nothing.
if(NOT dropin MATCHES "[\r\n]Type=exec[\r\n]")
    message(FATAL_ERROR
        "KDE portal drop-in must override Type=dbus so a private-bus session "
        "cannot have systemd terminate the identity-carrying backend")
endif()
# ADR-0191 corrects ADR-0170: systemd 261 does not accept a bare `BusName=`
# as "clear the inherited value" for this directive (unlike list-type
# settings such as ExecStart=). It logs "Invalid bus name, ignoring" on
# every unit (re)start and the value stays whatever the packaged unit set,
# confirmed live on qinda-top. BusName= is only mandatory for Type=dbus
# (systemd.service(5)), so under Type=exec the inherited value is inert and
# the drop-in must not declare it at all.
if(dropin MATCHES "[\r\n]BusName=[\r\n]")
    message(FATAL_ERROR
        "KDE portal drop-in must not declare an empty BusName=: systemd "
        "does not treat that as clearing the inherited value, it logs "
        "'Invalid bus name, ignoring' on every start instead (ADR-0191)")
endif()

message(STATUS "KDE portal compatibility activation contract is valid")

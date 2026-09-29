// SPDX-License-Identifier: GPL-3.0-or-later
#include "wire_types.h"
#include <QDBusMetaType>
#include <openssl/crypto.h>
namespace qindaqt::keyring::service {
QDBusArgument &operator<<(QDBusArgument &a, const WireSecret &s) {
    a.beginStructure(); a << s.session << s.parameters << s.value << s.contentType; a.endStructure(); return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, WireSecret &s) {
    a.beginStructure(); a >> s.session >> s.parameters >> s.value >> s.contentType; a.endStructure(); return a;
}
void registerWireTypes() {
    qDBusRegisterMetaType<WireSecret>(); qDBusRegisterMetaType<StringMap>();
    qDBusRegisterMetaType<SecretMap>(); qDBusRegisterMetaType<Paths>();
}
void wipe(QByteArray &bytes) noexcept {
    // Qt/D-Bus may own additional implicitly-shared copies; detach and wipe
    // only this owned buffer. Process dump suppression covers neither peers nor
    // framework buffers in other processes (ADR-0296).
    if (!bytes.isEmpty()) OPENSSL_cleanse(bytes.data(), static_cast<std::size_t>(bytes.size()));
    bytes.clear();
}
}

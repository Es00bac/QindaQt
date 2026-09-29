// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "wire_types.h"
#include <qindaqt/services/keyring/secure_buffer.h>
namespace qindaqt::keyring::service {
// Thread-confined session material; caller identity/lifetime belongs to broker.
// Throws sanitized errors, never input contents. Only negotiated AES key survives
// DH; EVP owns and retires DH private/intermediate provider allocations.
class SessionCrypto final {
public:
    QByteArray negotiate(const QString &algorithm, const QByteArray &input);
    SecureBuffer decode(WireSecret &wire, std::size_t maximum = 1024 * 1024) const;
    WireSecret encode(const QDBusObjectPath &session, const SecureBuffer &secret,
                      const QString &contentType) const;
private:
    bool encrypted_ = false;
    SecureBuffer key_;
};
}

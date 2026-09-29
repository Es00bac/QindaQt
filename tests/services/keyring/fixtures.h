// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring/collection_store.h>
#include <QFile>
#include <QDir>
#include <cstring>

namespace fixture {
using namespace qindaqt::keyring;
inline std::span<const unsigned char> password() {
    static constexpr unsigned char value[] = "unit-test-only-password";
    return {value, sizeof(value) - 1};
}
inline std::span<const unsigned char> otherPassword() {
    static constexpr unsigned char value[] = "unit-test-only-other-password";
    return {value, sizeof(value) - 1};
}
inline KdfParameters fastKdf() { return {8192, 1, 1}; }
inline Item item(std::string id = "item-one", std::string secret = "fixture-secret-only") {
    Item result;
    result.id = std::move(id);
    result.metadata = {"fixture label", "text/plain", "test-fixture", 17, 23};
    result.attributes = {{"service", "fixture-service"}, {"account", "fixture-account"}};
    result.secret = SecureBuffer(secret.size());
    std::memcpy(result.secret.bytes().data(), secret.data(), secret.size());
    return result;
}
inline QByteArray read(const QString &file) {
    QFile input(file);
    if (!input.open(QIODevice::ReadOnly)) return {};
    return input.readAll(); // Only encrypted fixture bytes.
}
inline bool write(const QString &file, const QByteArray &bytes) {
    QFile output(file);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return output.write(bytes) == bytes.size();
}
inline std::string root(const QString &temporary) { return (temporary + "/keyring").toStdString(); }
inline QString path(const QString &temporary, const QString &name = "login") {
    return temporary + "/keyring/" + name + ".qkr";
}
}

// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtDBus/QDBusArgument>

namespace QindaQt::BluetoothRadio {
inline constexpr auto kService = "org.qindaqt.BluetoothRadio1";
inline constexpr auto kPath = "/org/qindaqt/BluetoothRadio1";
inline constexpr auto kInterface = "org.qindaqt.BluetoothRadio1";
inline constexpr auto kIntentPath = "/org/qindaqt/BluetoothRadioIntent1";
inline constexpr auto kIntentInterface = "org.qindaqt.BluetoothRadioIntent1";
inline constexpr quint64 kRequestWindowMs = 2000;
inline constexpr qsizetype kMaxRequestsPerOwner = 512;

// These values contain selection and correlation, never device-open authority.
// The helper independently resolves the current service/caller and BlueZ owner,
// then joins the current Adapter1 to descriptor-pinned kernel HCI/rfkill state.
struct Request {
    QString nonce;
    QString bluezOwner;
    QString adapterPath;
    QString adapterAddress;
    QString initiatingCaller;
    quint64 deadlineBoottimeMs = 0;
    friend bool operator==(const Request &, const Request &) = default;
};

enum class Disposition : quint32 {
    VerifiedUnblocked = 0,
    NoWriteUnavailable = 1,
    Refused = 2,
    Uncertain = 3,
};

// NoWriteUnavailable is permitted only when no write syscall was attempted.
// Uncertain includes any possibly performed write without current verified
// identity/state. A full write count is not proof of a matching index/effect.
struct Result {
    QString nonce;
    Disposition disposition = Disposition::Uncertain;
    QString reasonCode;
    bool wireValid = true;
    friend bool operator==(const Result &, const Result &) = default;
};

[[nodiscard]] bool validRequest(const Request &request);
[[nodiscard]] bool validResult(const Result &result);
[[nodiscard]] quint64 boottimeMilliseconds();
void registerDBusTypes();
QDBusArgument &operator<<(QDBusArgument &argument, const Request &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Request &value);
QDBusArgument &operator<<(QDBusArgument &argument, const Result &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Result &value);
} // namespace QindaQt::BluetoothRadio

Q_DECLARE_METATYPE(QindaQt::BluetoothRadio::Request)
Q_DECLARE_METATYPE(QindaQt::BluetoothRadio::Result)

Q_DECLARE_METATYPE(QindaQt::BluetoothRadio::Disposition)

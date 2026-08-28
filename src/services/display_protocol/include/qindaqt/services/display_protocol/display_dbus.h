// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_types.h>

#include <QtDBus/QDBusArgument>

namespace QindaQt::Display
{

struct DBusDecodeResult {
    bool accepted = false;
    QString reasonCode;
};

void registerDBusTypes();

// Raw operators exist for Qt metatype registration. Boundary adapters use
// these transactional wrappers: they first require the exact v1 static
// signature, decode into a temporary, and replace a previously accepted value
// only after complete semantic validation. Arguments are borrowed for the
// call; results and accepted destinations are caller-owned. Registration and
// decode occur on the adapter's owning thread.
[[nodiscard]] DBusDecodeResult decodeCandidateArgument(const QDBusArgument &argument,
                                                       Candidate &destination);
[[nodiscard]] DBusDecodeResult decodeSnapshotArgument(const QDBusArgument &argument,
                                                      Snapshot &destination);
[[nodiscard]] DBusDecodeResult decodeOperationResultArgument(const QDBusArgument &argument,
                                                             OperationResult &destination);

QDBusArgument &operator<<(QDBusArgument &argument, const Mode &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Mode &value);
QDBusArgument &operator<<(QDBusArgument &argument, const Output &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Output &value);
QDBusArgument &operator<<(QDBusArgument &argument, const CandidateOutput &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, CandidateOutput &value);
QDBusArgument &operator<<(QDBusArgument &argument, const Candidate &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Candidate &value);
QDBusArgument &operator<<(QDBusArgument &argument, const TransactionSummary &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, TransactionSummary &value);
QDBusArgument &operator<<(QDBusArgument &argument, const Snapshot &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Snapshot &value);
QDBusArgument &operator<<(QDBusArgument &argument, const OperationResult &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, OperationResult &value);

} // namespace QindaQt::Display

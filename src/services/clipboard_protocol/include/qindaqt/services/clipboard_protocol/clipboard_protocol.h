// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_model/clipboard_types.h>

#include <QtCore/QByteArray>
#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace QindaQt::Services::Clipboard {

inline constexpr quint32 kSchemaVersion = 1;
inline constexpr qsizetype kMaxReasonCodeUtf8Bytes = 64;
inline constexpr qsizetype kMaxRememberedRequestsPerCaller = 64;
inline constexpr qsizetype kMaxRememberedCallers = 64;
inline constexpr char kServiceName[] = "org.qindaqt.Clipboard1";
inline constexpr char kObjectPath[] = "/org/qindaqt/Clipboard1";
inline constexpr char kInterfaceName[] = "org.qindaqt.Clipboard1";

enum class ClientState : quint32 {
    Stopped,
    Starting,
    Ready,
    Unavailable,
};

enum class OperationKind : quint32 {
    Select,
    Delete,
    Clear,
    Copy,
};

enum class OperationStatus : quint32 {
    Succeeded,
    Rejected,
    Failed,
    Uncertain,
    Busy,
};

// Clipboard1 deliberately reuses the QCDL descriptor bytes. No D-Bus-specific
// entry shape is allowed to become a second metadata authority.
struct Snapshot {
    quint32 schemaVersion = kSchemaVersion;
    quint64 epoch = 0;
    quint32 generation = 0;
    quint64 revision = 0;
    bool historyEnabled = false;
    bool privacyAllowed = false;
    QByteArray descriptorList;
    bool wireValid = true;

    friend bool operator==(const Snapshot &, const Snapshot &) = default;
};

struct OperationRequest {
    OperationKind kind = OperationKind::Select;
    quint64 requestId = 0;
    quint64 expectedEpoch = 0;
    quint32 expectedGeneration = 0;
    quint64 expectedRevision = 0;
    ClipboardModel::EntryId entry;
    bool clearAll = false;

    friend bool operator==(const OperationRequest &, const OperationRequest &) = default;
};

struct OperationResult {
    OperationKind kind = OperationKind::Select;
    OperationStatus status = OperationStatus::Failed;
    quint64 requestId = 0;
    quint64 initiatingEpoch = 0;
    quint32 initiatingGeneration = 0;
    quint64 initiatingRevision = 0;
    quint64 observedEpoch = 0;
    quint32 observedGeneration = 0;
    quint64 observedRevision = 0;
    QString reasonCode;
    bool wireValid = true;

    friend bool operator==(const OperationResult &, const OperationResult &) = default;
};

} // namespace QindaQt::Services::Clipboard

Q_DECLARE_METATYPE(QindaQt::Services::Clipboard::ClientState)
Q_DECLARE_METATYPE(QindaQt::Services::Clipboard::OperationKind)
Q_DECLARE_METATYPE(QindaQt::Services::Clipboard::OperationStatus)
Q_DECLARE_METATYPE(QindaQt::Services::Clipboard::Snapshot)
Q_DECLARE_METATYPE(QindaQt::Services::Clipboard::OperationResult)

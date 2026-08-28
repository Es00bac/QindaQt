// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QKeySequence>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QtGlobal>

namespace QindaQt::AppShell {

Q_NAMESPACE

enum class ErrorCode {
    None = 0,
    InvalidArgument,
    DuplicateAction,
    UnknownAction,
    Unavailable,
    Busy,
    StaleRequest,
    Denied,
    Cancelled,
    BackendFailure,
    WrongThread,
};
Q_ENUM_NS(ErrorCode)

enum class IntegrationState {
    NotRequired = 0,
    Ready,
    Degraded,
    Unavailable,
};
Q_ENUM_NS(IntegrationState)

enum class PortalKind {
    OpenFile = 0,
    SaveFile,
    SelectFolder,
};
Q_ENUM_NS(PortalKind)

struct Error final {
    ErrorCode code = ErrorCode::None;
    QString message;
    bool recoverable = false;

    [[nodiscard]] bool ok() const { return code == ErrorCode::None; }
    [[nodiscard]] static Error success() { return {}; }
};

struct ActionSpec final {
    QString id;
    QString menuId;
    QString menuLabel;
    QString label;
    QString accessibleDescription;
    QKeySequence shortcut;
    int menuOrder = 0;
    int order = 0;
    bool enabled = true;
    bool checkable = false;
    bool checked = false;
    bool destructive = false;
};

struct PortalRequest final {
    quint64 id = 0;
    PortalKind kind = PortalKind::OpenFile;
    QString title;
    QString suggestedName;
    QStringList mimeTypes;
};

struct PortalResult final {
    quint64 requestId = 0;
    PortalKind kind = PortalKind::OpenFile;
    bool accepted = false;
    QList<QUrl> urls;
    Error error;
};

// AGENT-CONTRACT: These values may cross queued Qt connections, but they carry
// no QObject ownership. Strings and collections are bounded by the registry or
// coordinator before publication; receivers may retain independent copies.
constexpr qsizetype MaximumActionCount = 256;
constexpr qsizetype MaximumMenuCount = 32;
constexpr qsizetype MaximumIdentifierLength = 64;
constexpr qsizetype MaximumLabelLength = 128;
constexpr qsizetype MaximumDiagnosticLength = 512;
constexpr qsizetype MaximumMimeTypeCount = 32;
constexpr qsizetype MaximumPortalUrlCount = 32;

[[nodiscard]] Error makeError(ErrorCode code,
                              const QString &message,
                              bool recoverable = false);

} // namespace QindaQt::AppShell

Q_DECLARE_METATYPE(QindaQt::AppShell::Error)
Q_DECLARE_METATYPE(QindaQt::AppShell::PortalRequest)
Q_DECLARE_METATYPE(QindaQt::AppShell::PortalResult)

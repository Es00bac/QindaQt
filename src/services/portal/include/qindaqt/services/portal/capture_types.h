// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDBusArgument>
#include <QDBusVariant>
#include <QJsonObject>
#include <QStringList>
#include <QString>
#include <QVariantMap>
#include <optional>
namespace QindaQt::Services::Portal {
enum class CaptureKind { Screenshot, Color, Stream };
// persist offers the user a "remember" choice; restore preselects stable
// output names from validated restore data. Neither skips the user's consent.
struct CaptureRequest { CaptureKind kind = CaptureKind::Screenshot; QString app, parent, session, caller; bool interactive = false, modal = true; bool multiple = false; quint32 cursorMode = 1; bool persist = false; QStringList restore{}; };
struct CaptureColor { double red = 0, green = 0, blue = 0; };
struct CaptureCoordinate { qint32 first = 0, second = 0; };
struct CaptureStream { quint32 node = 0; QVariantMap properties; };
using CaptureStreams = QList<CaptureStream>;
// Standard (suv) restore_data: vendor, version, opaque payload. The frontend
// stores it in the PermissionStore screencast table and hands it back only
// to the same app; QindaQt payloads hold stable output names alone.
struct CaptureRestoreData { QString vendor; quint32 version = 0; QDBusVariant payload; };
QDBusArgument &operator<<(QDBusArgument &, const CaptureColor &);
const QDBusArgument &operator>>(const QDBusArgument &, CaptureColor &);
QDBusArgument &operator<<(QDBusArgument &, const CaptureCoordinate &);
const QDBusArgument &operator>>(const QDBusArgument &, CaptureCoordinate &);
QDBusArgument &operator<<(QDBusArgument &, const CaptureStream &);
const QDBusArgument &operator>>(const QDBusArgument &, CaptureStream &);
QDBusArgument &operator<<(QDBusArgument &, const CaptureRestoreData &);
const QDBusArgument &operator>>(const QDBusArgument &, CaptureRestoreData &);
// Value-only policy: no display, files, authorization or persistence. Refusals
// return false/empty optional; wire registration precedes adaptor export on the
// resident thread. Actual pixels/nodes and actor lifetimes belong to their ports.
void registerCaptureWireTypes();
std::optional<CaptureRequest> screenshotRequest(const QString &app, const QString &parent,
    const QVariantMap &options, bool color);
bool validScreenCastSelection(const QVariantMap &options);
QString captureCaller(const QString &requestPath);
bool validCapturePublication(CaptureKind, const QVariantMap &);
// QindaQt restore data -> bounded stable output names; foreign, malformed or
// other-version data yields an empty list (a fresh choice, never an error).
QStringList restoreOutputs(const QVariant &restoreData);
QVariant restoreDataFor(const QStringList &outputs);
QJsonObject captureFrame(const CaptureRequest &, const QString &directory, const QString &owner);
std::optional<CaptureRequest> captureRequestFromFrame(const QJsonObject &);
std::optional<QVariantMap> captureResults(CaptureKind kind, const QJsonObject &, const QString &directory);
}
Q_DECLARE_METATYPE(QindaQt::Services::Portal::CaptureColor)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::CaptureCoordinate)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::CaptureStream)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::CaptureStreams)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::CaptureRestoreData)

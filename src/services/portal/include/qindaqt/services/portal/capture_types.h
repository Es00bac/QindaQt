// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDBusArgument>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>
#include <optional>
namespace QindaQt::Services::Portal {
enum class CaptureKind { Screenshot, Color, Stream };
struct CaptureRequest { CaptureKind kind; QString app, parent, session, caller; bool interactive = false, modal = true; };
struct CaptureColor { double red = 0, green = 0, blue = 0; };
struct CaptureCoordinate { qint32 first = 0, second = 0; };
struct CaptureStream { quint32 node = 0; QVariantMap properties; };
using CaptureStreams = QList<CaptureStream>;
QDBusArgument &operator<<(QDBusArgument &, const CaptureColor &);
const QDBusArgument &operator>>(const QDBusArgument &, CaptureColor &);
QDBusArgument &operator<<(QDBusArgument &, const CaptureCoordinate &);
const QDBusArgument &operator>>(const QDBusArgument &, CaptureCoordinate &);
QDBusArgument &operator<<(QDBusArgument &, const CaptureStream &);
const QDBusArgument &operator>>(const QDBusArgument &, CaptureStream &);
void registerCaptureWireTypes();
std::optional<CaptureRequest> screenshotRequest(const QString &app, const QString &parent,
    const QVariantMap &options, bool color);
bool validScreenCastSelection(const QVariantMap &options);
QString captureCaller(const QString &requestPath);
bool validCapturePublication(CaptureKind, const QVariantMap &);
QJsonObject captureFrame(const CaptureRequest &, const QString &directory, const QString &owner);
std::optional<CaptureRequest> captureRequestFromFrame(const QJsonObject &);
std::optional<QVariantMap> captureResults(CaptureKind kind, const QJsonObject &, const QString &directory);
}
Q_DECLARE_METATYPE(QindaQt::Services::Portal::CaptureColor)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::CaptureCoordinate)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::CaptureStream)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::CaptureStreams)

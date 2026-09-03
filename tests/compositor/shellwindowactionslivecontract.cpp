// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellwindowactionslivecontract.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QSet>
#include <QXmlStreamReader>

#include <optional>

namespace QindaQt::Compositor::TestSupport {
namespace {

constexpr auto ServiceName = "org.qindaqt.Compositor";
constexpr auto ShellPath = "/org/qindaqt/CompositorShell";
constexpr auto ShellInterface = "org.qindaqt.CompositorShell1";

struct InterfaceMembers final
{
    QSet<QString> methods;
    QSet<QString> signalNames;

    friend bool operator==(const InterfaceMembers &,
                           const InterfaceMembers &) = default;
};

std::optional<InterfaceMembers> parseMembers(const QByteArray &xml,
                                             QString *failure)
{
    QXmlStreamReader reader(xml);
    InterfaceMembers members;
    bool found = false;
    bool inTarget = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()
            && reader.name() == QLatin1StringView("interface")) {
            inTarget = reader.attributes().value(QStringLiteral("name"))
                == QLatin1StringView(ShellInterface);
            found = found || inTarget;
        } else if (reader.isEndElement()
                   && reader.name() == QLatin1StringView("interface")) {
            inTarget = false;
        } else if (inTarget && reader.isStartElement()) {
            const QString name = reader.attributes()
                                     .value(QStringLiteral("name")).toString();
            if (reader.name() == QLatin1StringView("method")) {
                if (name.isEmpty() || members.methods.contains(name)) {
                    if (failure) *failure = QStringLiteral("duplicate live method");
                    return std::nullopt;
                }
                members.methods.insert(name);
            } else if (reader.name() == QLatin1StringView("signal")) {
                if (name.isEmpty() || members.signalNames.contains(name)) {
                    if (failure) *failure = QStringLiteral("duplicate live signal");
                    return std::nullopt;
                }
                members.signalNames.insert(name);
            }
        }
    }
    if (reader.hasError() || !found) {
        if (failure) *failure = QStringLiteral("interface XML is malformed or absent");
        return std::nullopt;
    }
    return members;
}

} // namespace

bool liveShellInterfaceMatchesDescriptor(QString *failure)
{
    QFile descriptor(QString::fromUtf8(QINDAQT_COMPOSITOR_SHELL_DESCRIPTOR));
    if (!descriptor.open(QIODevice::ReadOnly)) {
        if (failure) *failure = QStringLiteral("could not read shell descriptor");
        return false;
    }
    const auto expected = parseMembers(descriptor.readAll(), failure);
    if (!expected) return false;

    QDBusInterface introspection(
        QString::fromLatin1(ServiceName), QString::fromLatin1(ShellPath),
        QStringLiteral("org.freedesktop.DBus.Introspectable"),
        QDBusConnection::sessionBus());
    const QDBusReply<QString> reply = introspection.call(QStringLiteral("Introspect"));
    if (!reply.isValid()) {
        if (failure) {
            *failure = QStringLiteral("live introspection failed: %1")
                           .arg(reply.error().message());
        }
        return false;
    }
    const auto observed = parseMembers(reply.value().toUtf8(), failure);
    if (!observed || *observed != *expected) {
        if (failure && observed) {
            *failure = QStringLiteral(
                "live CompositorShell1 methods/signals differ from its descriptor");
        }
        return false;
    }
    if (failure) failure->clear();
    return true;
}

} // namespace QindaQt::Compositor::TestSupport

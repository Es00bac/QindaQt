// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// A strict org.kde.StatusNotifierItem that routes by interface header the way
// Wine's (and GDBus's) implementations do. Split from the lenient scripted
// fake in status_notifier_fake_item_test_support.h: that one is served through
// QtDBus, which accepts header-less calls by member name, so it can never
// catch a host that forgets the interface header.

#include "status_notifier_private_bus_test_support.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>

namespace QindaQt::StatusNotifier::TestSupport
{

// AGENT-NOTE: modelled on the Battle.net item observed live on 2026-09-25
// under Wine. It answers org.freedesktop.DBus.Properties Get/GetAll only when
// the call names that interface; a header-less call gets Wine's exact
// UnknownMethod reply. The property set is exactly Wine's: no IconThemePath,
// Overlay* or Attention* keys, WindowId typed INT32 ('i') rather than the
// UINT32 the spec suggests, and Menu is the "/NO_DBUSMENU" placeholder.
class StrictWineStatusNotifierItem final : public QDBusVirtualObject
{
public:
    // Header-less Properties calls refused; a fixed host must leave this 0.
    int emptyInterfaceRejections = 0;

    StrictWineStatusNotifierItem() { registerFakeWireTypes(); }

    [[nodiscard]] bool registerOn(QDBusConnection &connection, const QString &path)
    {
        return connection.registerVirtualObject(path, this);
    }

    [[nodiscard]] static QVariantMap wineProperties()
    {
        // Opaque mid-grey 16x16 in ARGB32 (network byte order) wire form.
        QByteArray argb;
        argb.reserve(16 * 16 * 4);
        for (int pixel = 0; pixel < 16 * 16; ++pixel) {
            argb.append(char(0xFF));
            argb.append(char(0x80));
            argb.append(char(0x80));
            argb.append(char(0x80));
        }
        FakePixmapWire icon;
        icon.width = 16;
        icon.height = 16;
        icon.argb = argb;
        FakeToolTipWire toolTip;
        toolTip.title = QStringLiteral("Battle.net");
        return {
            {QStringLiteral("Category"), QStringLiteral("ApplicationStatus")},
            {QStringLiteral("Id"), QStringLiteral("wine-0x100fe-0")},
            {QStringLiteral("Title"), QStringLiteral("Battle.net")},
            {QStringLiteral("Status"), QStringLiteral("Active")},
            {QStringLiteral("IconName"), QString()},
            {QStringLiteral("IconPixmap"), QVariant::fromValue(QList<FakePixmapWire>{icon})},
            {QStringLiteral("ToolTip"), QVariant::fromValue(toolTip)},
            {QStringLiteral("ItemIsMenu"), false},
            {QStringLiteral("WindowId"), QVariant::fromValue(qint32(0))},
            {QStringLiteral("Menu"),
             QVariant::fromValue(QDBusObjectPath(QStringLiteral("/NO_DBUSMENU")))},
        };
    }

    QString introspect(const QString &path) const override
    {
        Q_UNUSED(path);
        return QStringLiteral("<interface name=\"%1\"/>")
            .arg(QString::fromLatin1(kItemInterfaceName));
    }

    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override
    {
        if (message.type() != QDBusMessage::MethodCallMessage
            || message.interface()
                == QLatin1StringView("org.freedesktop.DBus.Introspectable")) {
            return false; // QtDBus answers Introspect from introspect().
        }
        if (message.interface().isEmpty()) {
            ++emptyInterfaceRejections;
            connection.send(message.createErrorReply(
                QDBusError::UnknownMethod,
                QStringLiteral("Method \"%1\" with signature \"%2\" on interface "
                               "\"(null)\" doesn't exist")
                    .arg(message.member(), message.signature())));
            return true;
        }
        if (message.interface() != QLatin1StringView(kPropertiesInterfaceName)) {
            // Intent methods are not exercised through this fake.
            connection.send(message.createErrorReply(
                QDBusError::UnknownMethod, QStringLiteral("not scripted")));
            return true;
        }
        const QVariantList arguments = message.arguments();
        const bool ownInterface =
            arguments.value(0).toString() == QLatin1StringView(kItemInterfaceName);
        const QVariantMap properties = wineProperties();
        if (message.member() == QLatin1StringView("GetAll")
            && message.signature() == QLatin1StringView("s") && ownInterface) {
            connection.send(message.createReply(QVariant::fromValue(properties)));
            return true;
        }
        const QString name = arguments.value(1).toString();
        if (message.member() == QLatin1StringView("Get")
            && message.signature() == QLatin1StringView("ss") && ownInterface
            && properties.contains(name)) {
            connection.send(message.createReply(
                QVariant::fromValue(QDBusVariant(properties.value(name)))));
            return true;
        }
        connection.send(message.createErrorReply(
            QDBusError::UnknownMethod, QStringLiteral("unsupported Properties call")));
        return true;
    }
};

} // namespace QindaQt::StatusNotifier::TestSupport

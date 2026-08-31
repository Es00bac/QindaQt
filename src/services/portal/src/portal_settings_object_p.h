// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "portal_dbus_types_p.h"

#include "qindaqt/services/portal/appearance_source.h"
#include "qindaqt/services/portal/resident_portal_service.h"

#include <QDBusContext>
#include <QDBusVariant>
#include <QObject>

#include <optional>

namespace QindaQt::Services::Portal::Private {

class PortalSettingsObject final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Settings")
    Q_CLASSINFO(
        "D-Bus Introspection",
        "<interface name=\"org.freedesktop.impl.portal.Settings\">"
        "<method name=\"ReadAll\"><arg name=\"namespaces\" type=\"as\" "
        "direction=\"in\"/><arg name=\"value\" type=\"a{sa{sv}}\" "
        "direction=\"out\"/></method>"
        "<method name=\"Read\"><arg name=\"namespace\" type=\"s\" "
        "direction=\"in\"/><arg name=\"key\" type=\"s\" direction=\"in\"/>"
        "<arg name=\"value\" type=\"v\" direction=\"out\"/></method>"
        "<signal name=\"SettingChanged\"><arg name=\"namespace\" type=\"s\"/>"
        "<arg name=\"key\" type=\"s\"/><arg name=\"value\" type=\"v\"/>"
        "</signal><property name=\"version\" type=\"u\" access=\"read\"/>"
        "</interface>")
    Q_PROPERTY(quint32 version READ version CONSTANT SCRIPTABLE true)

public:
    explicit PortalSettingsObject(AppearanceSource &source,
                                  QObject *parent = nullptr);

    [[nodiscard]] quint32 version() const noexcept;

public Q_SLOTS:
    Q_SCRIPTABLE PortalNamespaceMap ReadAll(const QStringList &namespaces);
    Q_SCRIPTABLE QDBusVariant Read(const QString &namespaceName,
                                   const QString &key);

Q_SIGNALS:
    Q_SCRIPTABLE void SettingChanged(const QString &namespaceName,
                                     const QString &key,
                                     const QDBusVariant &value);

private:
    void handleSourceChanged();
    [[nodiscard]] bool validateReadAllInput(const QStringList &namespaces,
                                            QString *error) const;
    [[nodiscard]] bool matchesAppearance(const QStringList &namespaces) const;
    [[nodiscard]] QVariant valueFor(const AppearancePolicy &policy,
                                    const QString &key) const;
    [[nodiscard]] QVariantMap valuesFor(const AppearancePolicy &policy) const;
    void emitChanged(const QString &key, const QVariant &value);

    AppearanceSource &m_source;
    std::optional<AppearancePolicy> m_lastSignalled;
};

} // namespace QindaQt::Services::Portal::Private

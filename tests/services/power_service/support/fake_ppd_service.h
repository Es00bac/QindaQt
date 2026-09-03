// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusVirtualObject>

namespace QindaQt::Tests {

// Fake power-profiles-daemon implemented as a QDBusVirtualObject. Modern mode
// owns org.freedesktop.UPower.PowerProfiles and answers the modern interface;
// legacy mode owns only net.hadess.PowerProfiles and answers only the legacy
// interface with the deprecated Holds key, which exercises the adapter's name
// and interface fallbacks.
class FakePpdService final : public QDBusVirtualObject
{
public:
    struct HoldSpec {
        QString profile;
        QString application;
        QString reason;
    };

    struct HoldRequest {
        QString profile;
        QString reason;
        QString application;
        QString appId;
        QString holdPath;
    };

    FakePpdService(const QDBusConnection &connection, bool legacyOnly,
                   QObject *parent = nullptr);
    ~FakePpdService() override;

    bool registerService();
    void unregisterService();

    void setProfiles(const QStringList &profileIds);
    void setActiveProfile(const QString &profileId);
    void setHolds(const QList<HoldSpec> &holds);
    void setRejectSetProfile(bool reject);
    void emitPropertiesChanged();

    QString introspect(const QString &path) const override;
    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override;

    QStringList setProfileRequests;
    QList<HoldRequest> holdRequests;
    QStringList releaseRequests;
    int nextHoldNumber = 0;

private:
    [[nodiscard]] QString busName() const;
    [[nodiscard]] QString interfaceName() const;
    [[nodiscard]] QString holdsKey() const;
    [[nodiscard]] QVariant profilesValue() const;
    [[nodiscard]] QVariant holdsValue() const;
    void sendError(const QDBusMessage &message, const QString &name,
                   const QString &text);

    QDBusConnection m_connection;
    QStringList m_profiles;
    QString m_activeProfile;
    QList<HoldSpec> m_holds;
    bool m_legacyOnly;
    bool m_rejectSetProfile = false;
};

} // namespace QindaQt::Tests

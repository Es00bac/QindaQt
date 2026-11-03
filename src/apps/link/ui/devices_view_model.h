// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "json_list_model.h"
#include "link_api.h"

#include <qindaqt/services/link_client/link_client.h>

#include <QtCore/QObject>
#include <QtCore/QVariantMap>

#include <functional>

namespace QindaQt::LinkApp {

// Devices page: the paired phone, pairing requests, unpair and "Set up phone over USB".
//
// Ownership: borrows the api, which outlives it. Threading: GUI thread. Error behavior: failures
// become `error` text; nothing throws.
// AGENT-GUARD: ConfirmPairing is only ever sent from accept()/reject(), i.e. from a click on the
// code shown here. Never auto-accept, and never accept from a signal handler.
class DevicesViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY changed FINAL)
    Q_PROPERTY(bool hasDevice READ hasDevice NOTIFY changed FINAL)
    Q_PROPERTY(QVariantMap device READ device NOTIFY changed FINAL)
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY changed FINAL)
    Q_PROPERTY(bool phoneReady READ phoneReady NOTIFY changed FINAL)
    Q_PROPERTY(QStringList capabilities READ capabilities NOTIFY changed FINAL)
    Q_PROPERTY(QString error READ error NOTIFY changed FINAL)
    Q_PROPERTY(QObject *pairings READ pairings CONSTANT FINAL)
    Q_PROPERTY(QObject *setupSteps READ setupSteps CONSTANT FINAL)
    Q_PROPERTY(bool setupBusy READ setupBusy NOTIFY setupChanged FINAL)
    Q_PROPERTY(QString setupMessage READ setupMessage NOTIFY setupChanged FINAL)
    Q_PROPERTY(QString adbSerial READ adbSerial NOTIFY setupChanged FINAL)
public:
    using Clock = std::function<qint64()>;
    explicit DevicesViewModel(LinkApi &api, QObject *parent = nullptr);
    void setClock(Clock clock) { m_clock = std::move(clock); }

    [[nodiscard]] bool serviceAvailable() const { return m_api.available(); }
    [[nodiscard]] bool hasDevice() const { return m_hasDevice; }
    [[nodiscard]] QVariantMap device() const { return m_device; }
    [[nodiscard]] QString deviceId() const { return m_device.value(QStringLiteral("id")).toString(); }
    [[nodiscard]] bool phoneReady() const { return m_device.value(QStringLiteral("ready")).toBool(); }
    [[nodiscard]] QStringList capabilities() const { return m_device.value(QStringLiteral("caps")).toStringList(); }
    [[nodiscard]] QString error() const { return m_error; }
    [[nodiscard]] JsonListModel *pairings() { return &m_pairings; }
    [[nodiscard]] JsonListModel *setupSteps() { return &m_steps; }
    [[nodiscard]] bool setupBusy() const { return m_setupBusy; }
    [[nodiscard]] QString setupMessage() const { return m_setupMessage; }
    [[nodiscard]] QString adbSerial() const { return m_adbSerial; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void accept(const QString &id) { decide(id, true); }
    Q_INVOKABLE void reject(const QString &id) { decide(id, false); }
    // The QML dialog is the confirmation; this performs the unpair it asked for.
    Q_INVOKABLE void unpair(const QString &id);
    Q_INVOKABLE void setupPhone();
    Q_INVOKABLE void checkSetup();

Q_SIGNALS:
    void changed();
    void setupChanged();
    void deviceIdChanged(const QString &id);

private:
    void applyDevices(const QString &json);
    void applyPending(const QString &json);
    void addPairing(const QString &id, const QString &name, const QString &model, const QString &code);
    void decide(const QString &id, bool accept);
    void dropStalePairings();
    void withSerial(std::function<void(const QString &serial)> use);
    void showReport(const QString &json, bool isStatus);
    void setError(const QString &error);
    [[nodiscard]] QVariantMap describe(const Link::LinkDevice &d) const;

    LinkApi &m_api;
    Clock m_clock;
    JsonListModel m_pairings{{QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("model"), QStringLiteral("code")}};
    JsonListModel m_steps{{QStringLiteral("id"), QStringLiteral("label"), QStringLiteral("ok"), QStringLiteral("outcome")}};
    QList<Link::LinkDevice> m_devices;
    QVariantMap m_device;
    bool m_hasDevice = false;
    QString m_error;
    bool m_setupBusy = false;
    QString m_setupMessage;
    QString m_adbSerial;
    quint64 m_generation = 0;
};

} // namespace QindaQt::LinkApp

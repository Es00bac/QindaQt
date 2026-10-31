// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/firewall_client/firewall_client.h>
#include <qindaqt/shell/firewall_applet/firewall_applet_types.h>

#include <QtCore/QObject>

namespace QindaQt::Shell::FirewallApplet
{

// Shell-private adapter from the public FirewallClient to bounded QML
// values (ADR-0431, docs/wiki/shell/firewall-applet.md). It borrows a
// same-thread client owned by shell composition; the client must outlive
// the controller. The applet requests only `security.read`: every
// Q_INVOKABLE here is Dismiss/ConfirmMine/OpenWindow or a read, never a
// block/allow/rule change, so there is nothing a `security.control`-shaped
// capability would need to gate.
//
// AGENT-CONTRACT: when `securityReadGranted` is false, `client` may be
// null (shell composition constructs no FirewallClient for a denied
// grant, matching the Agent Usage applet) and this controller presents
// permanently as Unavailable, never touching `client`.
class FirewallAppletController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY modelChanged)
    Q_PROPERTY(QString stateText READ stateText NOTIFY modelChanged)
    Q_PROPERTY(QString iconColorRole READ iconColorRole NOTIFY modelChanged)
    Q_PROPERTY(quint32 alertCount READ alertCount NOTIFY modelChanged)
    Q_PROPERTY(bool persistentAttention READ persistentAttention NOTIFY modelChanged)
    Q_PROPERTY(bool hasAlert READ hasAlert NOTIFY modelChanged)
    Q_PROPERTY(QString alertTitle READ alertTitle NOTIFY modelChanged)
    Q_PROPERTY(QString alertSummary READ alertSummary NOTIFY modelChanged)
    Q_PROPERTY(QString alertMeaning READ alertMeaning NOTIFY modelChanged)
    Q_PROPERTY(QString alertAdvice READ alertAdvice NOTIFY modelChanged)
    Q_PROPERTY(bool alertCanConfirm READ alertCanConfirm NOTIFY modelChanged)

public:
    FirewallAppletController(Firewall::Client::FirewallClient *client,
                             bool securityReadGranted, QObject *parent = nullptr);
    ~FirewallAppletController() override;

    [[nodiscard]] QString state() const { return toString(m_model.state); }
    [[nodiscard]] QString stateText() const { return m_model.stateText; }
    [[nodiscard]] QString iconColorRole() const { return m_model.iconColorRole; }
    [[nodiscard]] quint32 alertCount() const noexcept { return m_model.alertCount; }
    [[nodiscard]] bool persistentAttention() const noexcept
    {
        return m_model.persistentAttention;
    }
    [[nodiscard]] bool hasAlert() const noexcept { return m_model.hasAlert; }
    [[nodiscard]] QString alertTitle() const { return m_model.alert.title; }
    [[nodiscard]] QString alertSummary() const { return m_model.alert.summary; }
    [[nodiscard]] QString alertMeaning() const { return m_model.alert.meaning; }
    [[nodiscard]] QString alertAdvice() const { return m_model.alert.advice; }
    [[nodiscard]] bool alertCanConfirm() const noexcept
    {
        return m_model.hasAlert && m_model.alert.canConfirm;
    }

    [[nodiscard]] const FirewallAppletModel &model() const noexcept { return m_model; }

    // "Open QindaFirewall": OpenWindow(topAlert id), or OpenWindow("") when
    // there is no open alert (the documented overview target).
    Q_INVOKABLE void openWindow();
    // "Got it, hide this": Dismiss(topAlert id). No-op without an alert.
    Q_INVOKABLE void dismissTopAlert();
    // "This was me": ConfirmMine(topAlert id). No-op without an alert, or
    // when the current alert does not offer it (canConfirm false).
    Q_INVOKABLE void confirmTopAlertMine();

Q_SIGNALS:
    void modelChanged();

private:
    void reproject();

    Firewall::Client::FirewallClient *m_client = nullptr;
    bool m_granted = false;
    FirewallAppletModel m_model;
};

} // namespace QindaQt::Shell::FirewallApplet

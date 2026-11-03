// SPDX-License-Identifier: GPL-3.0-or-later
// Hub + scripted phone + fake portal + fake InputActions1, all private to the
// test. See fake_portal.h for why no test can reach the real portal.
#pragma once

#include "fake_input_actions.h"
#include "fake_portal.h"
#include "hub_fixture.h"

#include <qindaqt/services/link_input/input_feature.h>
#include <qindaqt/services/link_macros/macro_catalog.h>
#include <qindaqt/services/link_macros/macro_feature.h>
#include <qindaqt/services/link_macros/portal_macro_output.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>

namespace QindaQt::Link::Test {

struct InputFixture {
    InputFixture()
        : portalName(QStringLiteral("org.qindaqt.test.FakePortal%1").arg(QCoreApplication::applicationPid())),
          actionsName(QStringLiteral("org.qindaqt.test.FakeInputActions%1").arg(QCoreApplication::applicationPid())),
          fakePortal(portalName), fakeActions(actionsName),
          portal(QDBusConnection::sessionBus(), options()),
          input(portal, gate), output(portal, gate),
          catalog(QDBusConnection::sessionBus(), actionsName), macros(hub.hub, catalog, input, output)
    {
        hub.hub.addFeature(&input);
        hub.hub.addFeature(&macros);
        catalog.start();
    }

    PortalOptions options()
    {
        PortalOptions o;
        o.service = portalName;
        o.tokenFile = tokens.path() + QStringLiteral("/state/restore-token");
        o.stepTimeoutMs = 3000;
        return o;
    }
    [[nodiscard]] bool usable() const { return fakePortal.ok() && fakeActions.ok() && hub.listening; }
    bool pairPhone(ScriptedPhone &phone) { return hub.pair(phone, {QStringLiteral("input"), QStringLiteral("macro")}); }
    [[nodiscard]] QString tokenFile() const { return tokens.path() + QStringLiteral("/state/restore-token"); }

    // Sends input.begin and waits for the reply; returns the reply type (ok/error).
    static void begin(ScriptedPhone &phone, qint64 id = 5)
    {
        Envelope e;
        e.t = QStringLiteral("input.begin");
        e.id = id;
        phone.send(e);
    }
    static Envelope frame(const QString &t, QJsonObject body = {}, qint64 id = 0)
    {
        Envelope e;
        e.t = t;
        e.id = id;
        e.b = std::move(body);
        return e;
    }
    bool callSeen(const QString &prefix) const
    {
        for (const QString &c : fakePortal.calls) {
            if (c.startsWith(prefix)) return true;
        }
        return false;
    }
    int callCount(const QString &prefix) const
    {
        int n = 0;
        for (const QString &c : fakePortal.calls) n += c.startsWith(prefix) ? 1 : 0;
        return n;
    }

    QTemporaryDir tokens;
    QString portalName, actionsName;
    FakePortal fakePortal;
    FakeInputActions fakeActions;
    HubFixture hub;
    StaticInputGate gate{true};
    PortalInputSession portal;
    InputFeature input;
    Macros::PortalMacroOutput output;
    Macros::MacroCatalog catalog;
    Macros::MacroFeature macros;
};

} // namespace QindaQt::Link::Test

// SPDX-License-Identifier: GPL-3.0-or-later
// Hub + scripted phone + fake portal + settable focus store, all private to the test. The portal
// is the fake from fake_portal.h, so no test can reach the real RemoteDesktop portal.
#pragma once

#include "fake_portal.h"
#include "hub_fixture.h"

#include <qindaqt/services/link_focus/focus_store.h>
#include <qindaqt/services/link_input/input_feature.h>
#include <qindaqt/services/link_vault/vault_typing_feature.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

namespace QindaQt::Link::Test {

struct VaultFixture {
    VaultFixture()
        : portalName(QStringLiteral("org.qindaqt.test.FakeVaultPortal%1").arg(QCoreApplication::applicationPid())),
          fakePortal(portalName), portal(QDBusConnection::sessionBus(), options()),
          input(portal, gate), vault(portal, gate, focus)
    {
        focus.setClock([this] { return clockMs; }); // tests move time themselves; the fact never goes stale on its own
        vault.setInputInUse([this] { return !input.controller().isEmpty(); });
        QObject::connect(&vault, &VaultTypingFeature::portalBorrowedChanged, &input, &InputFeature::setPortalBorrowed);
        hub.hub.addFeature(&input);
        hub.hub.addFeature(&vault);
    }
    PortalOptions options()
    {
        PortalOptions o;
        o.service = portalName;
        o.tokenFile = tokens.path() + QStringLiteral("/state/restore-token");
        o.stepTimeoutMs = 3000;
        return o;
    }
    [[nodiscard]] bool usable() const { return fakePortal.ok() && hub.listening; }
    bool pairPhone(ScriptedPhone &phone, const QStringList &caps = {QStringLiteral("vault")})
    {
        return hub.pair(phone, caps);
    }
    // The shell's side: publishes a focus fact.
    bool publish(const QString &uuid, quint64 revision, const QString &title = QStringLiteral("Terminal"),
                 const QString &app = QStringLiteral("org.example.term"))
    {
        return focus.publish(QJsonDocument(QJsonObject{{QStringLiteral("uuid"), uuid},
                                                       {QStringLiteral("appId"), app},
                                                       {QStringLiteral("title"), title},
                                                       {QStringLiteral("revision"), double(revision)}})
                                 .toJson(QJsonDocument::Compact)) != FocusStore::Result::Rejected;
    }
    static Envelope frame(const QString &t, QJsonObject body, qint64 id)
    {
        Envelope e;
        e.t = t;
        e.id = id;
        e.b = std::move(body);
        return e;
    }
    // desktop.focus round trip; returns the token ("" when the desktop answered an error).
    static QString askFocus(ScriptedPhone &phone, qint64 id, QString *app = nullptr, QString *title = nullptr)
    {
        phone.send(frame(QStringLiteral("desktop.focus"), {}, id));
        Envelope r;
        if (!phone.waitFor(QStringLiteral("desktop.focus"), &r) || r.re != id) return {};
        if (app != nullptr) *app = r.b.value(QStringLiteral("app")).toString();
        if (title != nullptr) *title = r.b.value(QStringLiteral("title")).toString();
        return r.b.value(QStringLiteral("token")).toString();
    }
    static QJsonObject typeBody(const QString &token, const QJsonArray &steps)
    {
        return QJsonObject{{QStringLiteral("token"), token}, {QStringLiteral("steps"), steps}};
    }
    static QJsonObject text(const QString &t) { return QJsonObject{{QStringLiteral("text"), t}}; }
    static QJsonObject key(const QString &k) { return QJsonObject{{QStringLiteral("key"), k}}; }
    static QJsonObject delay(int ms) { return QJsonObject{{QStringLiteral("delayMs"), ms}}; }
    // The keysyms the fake portal saw pressed, in order.
    [[nodiscard]] QList<qint64> pressed() const
    {
        QList<qint64> out;
        for (const QString &c : fakePortal.calls) {
            const QStringList p = c.split(QLatin1Char(':'));
            if (p.value(0) == QLatin1String("NotifyKeyboardKeysym") && p.value(2) == QLatin1String("1")) out << p.value(1).toLongLong();
        }
        return out;
    }
    // Portal calls are asynchronous D-Bus calls and trail the phone's reply: wait until none arrived for 80 ms.
    void settle() const
    {
        qsizetype last = -1;
        for (int quiet = 0; quiet < 80 && last != -2;) {
            QTest::qWait(10);
            const qsizetype now = fakePortal.calls.size();
            quiet = now == last ? quiet + 10 : 0;
            last = now;
        }
    }
    [[nodiscard]] QList<qint64> settledPressed() const
    {
        settle();
        return pressed();
    }
    // Presses minus releases per keysym: all zero when nothing is left held.
    [[nodiscard]] bool nothingHeld() const
    {
        settle();
        QHash<qint64, int> held;
        for (const QString &c : fakePortal.calls) {
            const QStringList p = c.split(QLatin1Char(':'));
            if (p.value(0) != QLatin1String("NotifyKeyboardKeysym")) continue;
            held[p.value(1).toLongLong()] += p.value(2) == QLatin1String("1") ? 1 : -1;
        }
        for (int v : held) {
            if (v != 0) return false;
        }
        return true;
    }

    qint64 clockMs = 0;
    QTemporaryDir tokens;
    QString portalName;
    FakePortal fakePortal;
    HubFixture hub;
    StaticInputGate gate{true};
    FocusStore focus;
    PortalInputSession portal;
    InputFeature input;
    VaultTypingFeature vault;
};

} // namespace QindaQt::Link::Test

// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/private_bus.h"
#include <qindaqt/services/portal/email_adaptor.h>
#include <qindaqt/services/portal/email_policy.h>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QUrlQuery>
#include <QtTest>
using namespace QindaQt::Services::Portal;
using namespace QindaQt::Services::ApplicationUri;
class Opener final : public ApplicationUriOpener {
public:
    using ApplicationUriOpener::ApplicationUriOpener;
    void open(quint64 t, const QUrl &u, const QString &) override { token = t; uri = u; ++opens; }
    void cancel(quint64 t) override { if (t == token) { token = 0; ++cancels; } }
    quint64 token = 0; QUrl uri; int opens = 0, cancels = 0;
};
class EmailTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init() {
        fixture = std::make_unique<PortalPrivateBus>(); QVERIFY(fixture->start());
        backend = fixture->connect(); frontend = fixture->connect();
        QVERIFY(frontend->registerService(QStringLiteral("org.freedesktop.portal.Desktop")));
        registry = std::make_unique<RequestRegistry>(*backend); opener = std::make_unique<Opener>(); host = std::make_unique<QObject>();
        allowed = true; new EmailAdaptor(*host, *registry, *opener, [this] { return allowed; });
        QVERIFY(backend->registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), host.get(), QDBusConnection::ExportAdaptors));
        QVERIFY(backend->registerService(QStringLiteral("org.test.Portal")));
    }
    void cleanup() { host.reset(); registry.reset(); opener.reset(); backend.reset(); frontend.reset(); fixture.reset(); }
    void nativeDraftResult() {
        auto reply = call({{QStringLiteral("addresses"), QStringList{QStringLiteral("test@example.invalid")}},
            {QStringLiteral("subject"), QStringLiteral("Synthetic draft")}});
        QTRY_COMPARE(opener->opens, 1); QCOMPARE(opener->uri.scheme(), QStringLiteral("mailto"));
        Q_EMIT opener->completed(opener->token, UriOpenResult::Started);
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *reply;
        QCOMPARE(result.argumentAt<0>(), 0U); QVERIFY(result.argumentAt<1>().isEmpty()); QCOMPARE(opener->cancels, 1);
    }
    void closeBeforeHandoff() {
        auto reply = call(); QTRY_COMPARE(opener->opens, 1); const auto token = opener->token;
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), path,
            QStringLiteral("org.freedesktop.impl.portal.Request"), QStringLiteral("Close"));
        QDBusPendingCallWatcher close(frontend->asyncCall(message)); QTRY_VERIFY(close.isFinished());
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *reply;
        QCOMPARE(result.argumentAt<0>(), 1U); QCOMPARE(opener->cancels, 1);
        Q_EMIT opener->completed(token, UriOpenResult::Started); QVERIFY(!registry->live(token));
    }
    void uncertaintyAndOwnerLoss() {
        auto reply = call(); QTRY_COMPARE(opener->opens, 1); allowed = false;
        Q_EMIT opener->completed(opener->token, UriOpenResult::Started);
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *reply; QCOMPARE(result.argumentAt<0>(), 2U);
        allowed = true; auto second = call(); QTRY_COMPARE(opener->opens, 2);
        QVERIFY(frontend->unregisterService(QStringLiteral("org.freedesktop.portal.Desktop")));
        QTRY_VERIFY(second->isFinished()); const QDBusPendingReply<quint32, QVariantMap> lost = *second; QCOMPARE(lost.argumentAt<0>(), 2U);
    }
    void unsupportedAttachmentsFailWithoutLaunch() {
        auto reply = call({{QStringLiteral("attachments"), QStringList{QStringLiteral("file:///synthetic-only")}}});
        QTRY_VERIFY(reply->isFinished()); const QDBusPendingReply<quint32, QVariantMap> result = *reply;
        QCOMPARE(result.argumentAt<0>(), 2U); QCOMPARE(opener->opens, 0);
    }
    void encodedLiteralHeaderPolicy() {
        QVERIFY(!emailDraft({}, {{QStringLiteral("subject"), QStringLiteral("safe\r\nBcc: injected")}}));
        const auto draft = emailDraft({}, {{QStringLiteral("subject"), QStringLiteral("literal %0D & $()")},
            {QStringLiteral("body"), QStringLiteral("First\nSecond")}, {QStringLiteral("cc"), QStringList{QStringLiteral("cc@example.invalid")}}});
        QVERIFY(draft.has_value()); const QUrlQuery query(draft->uri);
        QCOMPARE(query.queryItemValue(QStringLiteral("subject"), QUrl::FullyDecoded), QStringLiteral("literal %0D & $()"));
        QCOMPARE(query.queryItemValue(QStringLiteral("body"), QUrl::FullyDecoded), QStringLiteral("First\nSecond"));
        QVERIFY(!emailDraft({}, {{QStringLiteral("cc"), 2U}}));
    }
private:
    std::unique_ptr<QDBusPendingCallWatcher> call(const QVariantMap &options = {}) {
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.test.Portal"), QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.impl.portal.Email"), QStringLiteral("ComposeEmail"));
        message.setArguments({QVariant::fromValue(QDBusObjectPath(path)), QString{}, QString{}, options});
        return std::make_unique<QDBusPendingCallWatcher>(frontend->asyncCall(message));
    }
    const QString path = QStringLiteral("/org/freedesktop/portal/desktop/request/fixture/email");
    std::unique_ptr<PortalPrivateBus> fixture;
    std::unique_ptr<QDBusConnection> backend, frontend;
    std::unique_ptr<RequestRegistry> registry; std::unique_ptr<Opener> opener; std::unique_ptr<QObject> host; bool allowed = true;
};
QTEST_GUILESS_MAIN(EmailTest)
#include "tst_email.moc"

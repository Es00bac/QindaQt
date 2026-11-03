// SPDX-License-Identifier: GPL-3.0-or-later
// Threads and conversation view models over a scripted phone: paging merge, live refresh, the
// send flow with its per-session confirmation, send status and the local-row hand-over.
#include "conversation_view_model.h"
#include "fake_link_api.h"
#include "threads_view_model.h"

#include <QtCore/QSet>
#include <QtTest/QtTest>

using namespace QindaQt::LinkApp;

namespace {

const QString kDevice = QStringLiteral("dev");

QVariantMap threadOne()
{
    return {{QStringLiteral("threadId"), qint64(1)}, {QStringLiteral("title"), QStringLiteral("Sam Carter")},
            {QStringLiteral("addrs"), QStringList{QStringLiteral("+15550100")}},
            {QStringLiteral("names"), QStringList{QStringLiteral("Sam Carter")}}, {QStringLiteral("unread"), 0}};
}

QList<qint64> dates(const JsonListModel *model)
{
    QList<qint64> out;
    for (const QVariantMap &row : model->rows()) out << row.value(QStringLiteral("date")).toLongLong();
    return out;
}

} // namespace

class ConversationTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void pagesOlderMessagesWithoutDuplicates()
    {
        FakeLinkApi api;
        ConversationViewModel conv(api);
        conv.setDeviceId(kDevice);
        conv.openThread(threadOne());
        QTRY_COMPARE(conv.messages()->count(), 50);
        QVERIFY(conv.hasOlder());
        { const QList<qint64> d = dates(conv.messages()); QVERIFY(std::is_sorted(d.cbegin(), d.cend())); }
        QCOMPARE(conv.messages()->rows().constLast().value(QStringLiteral("id")).toLongLong(), 1999);

        conv.loadOlder();
        QTRY_COMPARE(conv.messages()->count(), 100);
        conv.loadOlder();
        QTRY_COMPARE(conv.messages()->count(), 132);
        QVERIFY(!conv.hasOlder());
        QSet<QString> keys;
        for (const QVariantMap &row : conv.messages()->rows()) keys.insert(row.value(QStringLiteral("key")).toString());
        QCOMPARE(keys.size(), 132);
        { const QList<qint64> d = dates(conv.messages()); QVERIFY(std::is_sorted(d.cbegin(), d.cend())); }
    }

    void olderPageIsPrependedAndTheNewestRefreshAppends()
    {
        FakeLinkApi api;
        ConversationViewModel conv(api);
        conv.setDeviceId(kDevice);
        conv.openThread(threadOne());
        QTRY_COMPARE(conv.messages()->count(), 50);
        QSignalSpy inserted(conv.messages(), &QAbstractItemModel::rowsInserted);
        conv.loadOlder();
        QTRY_COMPARE(conv.messages()->count(), 100);
        QCOMPARE(inserted.takeFirst().at(1).toInt(), 0);  // prepended: first inserted row is 0

        QJsonObject fresh = api.messages[1].first();
        fresh.insert(QStringLiteral("id"), 5000);
        fresh.insert(QStringLiteral("body"), QStringLiteral("a newer message"));
        fresh.insert(QStringLiteral("date"), fresh.value(QStringLiteral("date")).toDouble() + 60000);
        api.messages[1].prepend(fresh);
        inserted.clear();
        Q_EMIT api.threadsChanged(kDevice, QStringLiteral("{\"threads\":[1],\"all\":false,\"unread\":0}"));
        QTRY_COMPARE(conv.messages()->count(), 101);
        QCOMPARE(inserted.constFirst().at(1).toInt(), 100);  // appended at the end
    }

    void firstSendAsksForConfirmationLaterOnesDoNot()
    {
        FakeLinkApi api;
        ConversationViewModel conv(api);
        conv.setDeviceId(kDevice);
        conv.openThread(threadOne());
        QTRY_COMPARE(conv.messages()->count(), 50);
        QSignalSpy asked(&conv, &ConversationViewModel::confirmSendRequested);
        QSignalSpy accepted(&conv, &ConversationViewModel::sendAccepted);

        conv.requestSend(QStringLiteral("hello there"));
        QCOMPARE(asked.size(), 1);
        QVERIFY(asked.constFirst().at(0).toString().contains(QStringLiteral("+15550100")));
        QVERIFY(asked.constFirst().at(0).toString().contains(QStringLiteral("Sam Carter")));
        QVERIFY(!api.calls.contains(QStringLiteral("send")));  // nothing sent before the OK

        conv.confirmSend();
        QTRY_COMPARE(accepted.size(), 1);
        QCOMPARE(api.sentTo, (QList<QStringList>{{QStringLiteral("+15550100")}}));
        QCOMPARE(api.sentTexts, QStringList{QStringLiteral("hello there")});
        QVERIFY(conv.confirmedThisSession());

        conv.requestSend(QStringLiteral("second"));
        QCOMPARE(asked.size(), 1);  // no second prompt this session
        QTRY_COMPARE(accepted.size(), 2);
        QCOMPARE(api.sentTexts.size(), 2);
    }

    void confirmationIsPerRecipientSetNotPerSession()
    {
        FakeLinkApi api;
        ConversationViewModel conv(api);
        conv.setDeviceId(kDevice);
        QSignalSpy asked(&conv, &ConversationViewModel::confirmSendRequested);
        conv.openThread(threadOne());
        QTRY_COMPARE(conv.messages()->count(), 50);
        conv.requestSend(QStringLiteral("one"));
        conv.confirmSend();
        QTRY_COMPARE(api.sentTexts.size(), 1);
        conv.close();

        QVariantMap other = threadOne();
        other.insert(QStringLiteral("threadId"), qint64(2));
        other.insert(QStringLiteral("addrs"), QStringList{QStringLiteral("+15550177")});
        other.insert(QStringLiteral("names"), QStringList{QString()});
        conv.openThread(other);
        QTRY_VERIFY(!conv.loading());
        QVERIFY(!conv.confirmedThisSession());
        conv.requestSend(QStringLiteral("two"));
        QCOMPARE(asked.size(), 2);  // a different recipient set asks again
        QCOMPARE(api.sentTexts.size(), 1);
        conv.cancelSend();
        conv.close();

        conv.openThread(threadOne());  // the first set stays confirmed, however it is written
        QTRY_VERIFY(!conv.loading());
        QVERIFY(conv.confirmedThisSession());
        conv.requestSend(QStringLiteral("again"));
        QCOMPARE(asked.size(), 2);
        QTRY_COMPARE(api.sentTexts.size(), 2);
    }

    void partialFailureNamesUnsentRecipientsAndOffersARetryToThem()
    {
        FakeLinkApi api;
        ConversationViewModel conv(api);
        conv.setDeviceId(kDevice);
        QVariantMap group = threadOne();
        group.insert(QStringLiteral("addrs"), QStringList{QStringLiteral("+15550100"), QStringLiteral("+15550111"), QStringLiteral("+15550122")});
        group.insert(QStringLiteral("names"), QStringList{QStringLiteral("Sam Carter"), QStringLiteral("Mina Okafor"), QString()});
        conv.openThread(group);
        QTRY_COMPARE(conv.messages()->count(), 50);
        QSignalSpy asked(&conv, &ConversationViewModel::confirmSendRequested);
        conv.requestSend(QStringLiteral("group text"));
        conv.confirmSend();
        QTRY_COMPARE(conv.messages()->count(), 51);
        Q_EMIT api.sendStatus(kDevice, 100, QStringLiteral("failed"), QStringLiteral("no-service"), QStringLiteral("{\"unsent\":[1,2]}"));
        const QString text = conv.messages()->rows().constLast().value(QStringLiteral("statusText")).toString();
        QVERIFY(text.contains(QStringLiteral("Mina Okafor")));
        QVERIFY(text.contains(QStringLiteral("+15550122")));
        QVERIFY(!text.contains(QStringLiteral("Sam Carter")));
        QVERIFY(conv.retryAvailable());
        QVERIFY(conv.retryRecipients().contains(QStringLiteral("+15550111")));

        conv.retryToUnsent();
        QCOMPARE(asked.size(), 2);  // the narrower recipient set was never confirmed
        conv.confirmSend();
        QTRY_COMPARE(api.sentTo.size(), 2);
        QCOMPARE(api.sentTo.last(), (QStringList{QStringLiteral("+15550111"), QStringLiteral("+15550122")}));
        QCOMPARE(api.sentTexts.last(), QStringLiteral("group text"));
        QVERIFY(!conv.retryAvailable());
    }

    void sendErrorsHaveDistinctWordsAndStayUntilTheReaderActs()
    {
        FakeLinkApi api;
        ConversationViewModel conv(api);
        conv.setDeviceId(kDevice);
        conv.openThread(threadOne());
        QTRY_COMPARE(conv.messages()->count(), 50);
        api.sendError = QStringLiteral("rate-limited");
        conv.requestSend(QStringLiteral("x"));
        conv.confirmSend();
        QTRY_VERIFY(conv.sendError().contains(QStringLiteral("Too many texts")));
        QVERIFY(conv.retryAvailable());
        conv.refresh();  // a successful reload must not hide it
        QTRY_VERIFY(!conv.loading());
        QVERIFY(!conv.sendError().isEmpty());

        api.sendError = QStringLiteral("caller-denied");
        conv.retryToUnsent();
        QTRY_VERIFY(conv.sendError().contains(QStringLiteral("installed app")));
        QVERIFY(conv.retryAvailable());
        api.sendError = QStringLiteral("denied");
        conv.retryToUnsent();
        QTRY_VERIFY(conv.sendError().contains(QStringLiteral("SMS permission")));
        QVERIFY(!conv.sendError().contains(QStringLiteral("installed app")));
        conv.dismissRetry();
        QVERIFY(conv.sendError().isEmpty());
    }

    void cancellingTheConfirmationSendsNothing()
    {
        FakeLinkApi api;
        ConversationViewModel conv(api);
        conv.setDeviceId(kDevice);
        conv.openThread(threadOne());
        QTRY_COMPARE(conv.messages()->count(), 50);
        conv.requestSend(QStringLiteral("not this"));
        conv.cancelSend();
        conv.confirmSend();  // nothing pending any more
        QTest::qWait(50);
        QVERIFY(!api.calls.contains(QStringLiteral("send")));
        QVERIFY(!conv.confirmedThisSession());
    }

    void emptyOrOverlongTextIsNeverSent()
    {
        FakeLinkApi api;
        ConversationViewModel conv(api);
        conv.setDeviceId(kDevice);
        conv.openThread(threadOne());
        QTRY_COMPARE(conv.messages()->count(), 50);
        QSignalSpy asked(&conv, &ConversationViewModel::confirmSendRequested);
        conv.requestSend(QStringLiteral("   "));
        conv.requestSend(QString(1601, QLatin1Char('x')));
        QCOMPARE(asked.size(), 0);
        QVERIFY(!conv.sendError().isEmpty());
    }

    void sendStatusUpdatesTheLocalRowAndFailuresNameUnsentRecipients()
    {
        FakeLinkApi api;
        ConversationViewModel conv(api);
        conv.setDeviceId(kDevice);
        conv.openThread(threadOne());
        QTRY_COMPARE(conv.messages()->count(), 50);
        conv.requestSend(QStringLiteral("status test"));
        conv.confirmSend();
        QTRY_COMPARE(conv.messages()->count(), 51);
        auto last = [&] { return conv.messages()->rows().constLast(); };
        QCOMPARE(last().value(QStringLiteral("status")).toString(), QStringLiteral("sending"));
        QVERIFY(last().value(QStringLiteral("local")).toBool());

        Q_EMIT api.sendStatus(kDevice, 100, QStringLiteral("sent"), QString(), QString());
        QCOMPARE(last().value(QStringLiteral("status")).toString(), QStringLiteral("sent"));
        Q_EMIT api.sendStatus(kDevice, 100, QStringLiteral("delivered"), QString(), QString());
        QCOMPARE(last().value(QStringLiteral("status")).toString(), QStringLiteral("delivered"));

        conv.requestSend(QStringLiteral("will fail"));
        QTRY_COMPARE(conv.messages()->count(), 52);
        Q_EMIT api.sendStatus(kDevice, 101, QStringLiteral("failed"), QStringLiteral("no-service"),
                              QStringLiteral("{\"unsent\":[1,2]}"));
        QCOMPARE(last().value(QStringLiteral("status")).toString(), QStringLiteral("failed"));
        QVERIFY(last().value(QStringLiteral("statusText")).toString().contains(QStringLiteral("no mobile service")));
        // A status for another phone changes nothing.
        Q_EMIT api.sendStatus(QStringLiteral("other"), 101, QStringLiteral("delivered"), QString(), QString());
        QCOMPARE(last().value(QStringLiteral("status")).toString(), QStringLiteral("failed"));
    }

    void aProviderCopyReplacesTheLocalRow()
    {
        FakeLinkApi api;
        ConversationViewModel conv(api);
        conv.setDeviceId(kDevice);
        conv.openThread(threadOne());
        QTRY_COMPARE(conv.messages()->count(), 50);
        conv.requestSend(QStringLiteral("same words"));
        conv.confirmSend();
        QTRY_COMPARE(conv.messages()->count(), 51);

        QJsonObject copy = api.messages[1].first();
        copy.insert(QStringLiteral("id"), 7000);
        copy.insert(QStringLiteral("box"), QStringLiteral("sent"));
        copy.insert(QStringLiteral("body"), QStringLiteral("same words"));
        copy.insert(QStringLiteral("date"), double(QDateTime::currentMSecsSinceEpoch() + 1000));
        api.messages[1].prepend(copy);
        conv.refresh();
        QTRY_VERIFY(conv.messages()->indexOf(QStringLiteral("key"), QStringLiteral("sms:7000")) >= 0);
        QCOMPARE(conv.messages()->indexOf(QStringLiteral("local"), true), -1);
    }

    void loadsAnMmsImageOnceAsADataUrl()
    {
        FakeLinkApi api;
        ConversationViewModel conv(api);
        conv.setDeviceId(kDevice);
        conv.openThread(threadOne());
        QTRY_COMPARE(conv.messages()->count(), 50);
        const int row = conv.messages()->indexOf(QStringLiteral("key"), QStringLiteral("mms:40"));
        QVERIFY(row >= 0);
        QCOMPARE(conv.messages()->get(row).value(QStringLiteral("imageState")).toString(), QStringLiteral("none"));
        conv.loadImage(QStringLiteral("mms:40"));
        conv.loadImage(QStringLiteral("mms:40"));  // a second call must not fetch again
        QTRY_COMPARE(conv.messages()->get(row).value(QStringLiteral("imageState")).toString(), QStringLiteral("ready"));
        QVERIFY(conv.messages()->get(row).value(QStringLiteral("image")).toString().startsWith(QStringLiteral("data:image/png;base64,")));
        QCOMPARE(api.calls.count(QStringLiteral("getPart")), 1);
    }

    void threadListPagesSearchesAndClearsUnread()
    {
        FakeLinkApi api;
        ThreadsViewModel threads(api);
        threads.setDeviceId(kDevice);
        QTRY_COMPARE(threads.threads()->count(), 24);
        QVERIFY(!threads.hasMore());
        QCOMPARE(threads.unread(), 3);

        threads.setSearchText(QStringLiteral("lena"));
        QCOMPARE(threads.threads()->count(), 1);
        threads.setSearchText(QStringLiteral("22395"));  // a number, matched on the raw address
        QCOMPARE(threads.threads()->count(), 1);
        threads.setSearchText(QString());
        QCOMPARE(threads.threads()->count(), 24);

        threads.clearUnread(1);
        QCOMPARE(threads.unread(), 1);
    }

    void threadListLoadsMoreWhenThePhoneHasMore()
    {
        FakeLinkApi api;
        for (int i = 100; i < 160; ++i) {
            api.threads.append(QJsonObject{{QStringLiteral("id"), i}, {QStringLiteral("addrs"), QJsonArray{QStringLiteral("+1555")}},
                                           {QStringLiteral("names"), QJsonArray{QString()}}, {QStringLiteral("snippet"), QStringLiteral("x")},
                                           {QStringLiteral("date"), double(1000 - i)}, {QStringLiteral("unread"), 0},
                                           {QStringLiteral("count"), 1}});
        }
        ThreadsViewModel threads(api);
        threads.setDeviceId(kDevice);
        QTRY_COMPARE(threads.loadedCount(), 50);
        QVERIFY(threads.hasMore());
        threads.loadMore();
        QTRY_COMPARE(threads.loadedCount(), 84);
        QVERIFY(!threads.hasMore());
    }

    void aRefreshedFirstPageDropsDeletedThreads()
    {
        FakeLinkApi api;
        ThreadsViewModel threads(api);
        threads.setDeviceId(kDevice);
        QTRY_COMPARE(threads.threads()->count(), 24);
        api.threads.removeAt(2);  // deleted on the phone
        threads.reload();
        QTRY_COMPARE(threads.threads()->count(), 23);
        QCOMPARE(threads.threads()->indexOf(QStringLiteral("threadId"), qint64(3)), -1);
    }

    void aDeniedPhoneShowsAReasonNotAnEmptyList()
    {
        FakeLinkApi api;
        api.errorForAll = QStringLiteral("denied");
        ThreadsViewModel threads(api);
        threads.setDeviceId(kDevice);
        QTRY_VERIFY(!threads.error().isEmpty());
        QVERIFY(threads.error().contains(QStringLiteral("Set up phone")));
    }
};

QTEST_GUILESS_MAIN(ConversationTest)
#include "tst_conversation.moc"

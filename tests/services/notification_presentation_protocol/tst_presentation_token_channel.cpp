// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_token_channel.h"

#include <QElapsedTimer>
#include <QtTest>

#include <cerrno>
#include <unistd.h>

using namespace QindaQt::Services::NotificationPresentation;

class PresentationTokenChannelTests final : public QObject {
    Q_OBJECT

private slots:
    void roundTripsAndConsumesDescriptors();
    void rejectsTruncatedMalformedAndOversizedRecords();
    void timesOutOnASilentOpenWriter();
    void rejectsInvalidDescriptors();
};

void PresentationTokenChannelTests::roundTripsAndConsumesDescriptors()
{
    int descriptors[2] = {-1, -1};
    QCOMPARE(::pipe(descriptors), 0);
    QString error;
    auto token = PresentationAccessToken::fromHex(
        QString(64, QLatin1Char('d')), &error);
    QVERIFY(token.has_value());
    QVERIFY2(PresentationTokenChannel::writeAndClose(
                 descriptors[1], *token, &error),
             qPrintable(error));
    const auto result = PresentationTokenChannel::readAndClose(descriptors[0]);
    QVERIFY2(result.ok(), qPrintable(result.message));
    QVERIFY(result.token->matches(QString(64, QLatin1Char('d'))));
    errno = 0;
    QCOMPARE(::close(descriptors[0]), -1);
    QCOMPARE(errno, EBADF);
    errno = 0;
    QCOMPARE(::close(descriptors[1]), -1);
    QCOMPARE(errno, EBADF);
}

void PresentationTokenChannelTests::rejectsTruncatedMalformedAndOversizedRecords()
{
    const auto check = [](const QByteArray &record) {
        int descriptors[2] = {-1, -1};
        QCOMPARE(::pipe(descriptors), 0);
        QCOMPARE(::write(descriptors[1], record.constData(),
                         std::size_t(record.size())),
                 ssize_t(record.size()));
        QCOMPARE(::close(descriptors[1]), 0);
        const auto result = PresentationTokenChannel::readAndClose(descriptors[0]);
        QVERIFY(!result.ok());
        QCOMPARE(result.status, TokenChannelStatus::InvalidRecord);
        QVERIFY(!result.message.contains(QString::fromLatin1(record)));
    };
    check(QByteArray(64, 'a'));
    check(QByteArray(64, 'z') + '\n');
    check(QByteArray(66, 'a'));
}

void PresentationTokenChannelTests::timesOutOnASilentOpenWriter()
{
    int descriptors[2] = {-1, -1};
    QCOMPARE(::pipe(descriptors), 0);
    QElapsedTimer elapsed;
    elapsed.start();
    const auto result = PresentationTokenChannel::readAndClose(descriptors[0]);
    QCOMPARE(result.status, TokenChannelStatus::ReadTimedOut);
    QVERIFY(elapsed.elapsed() >= 1'500);
    QVERIFY(elapsed.elapsed() < 5'000);
    QCOMPARE(::close(descriptors[1]), 0);
}

void PresentationTokenChannelTests::rejectsInvalidDescriptors()
{
    const auto result = PresentationTokenChannel::readAndClose(-1);
    QCOMPARE(result.status, TokenChannelStatus::InvalidDescriptor);
    QString error;
    auto token = PresentationAccessToken::generate();
    QVERIFY(!PresentationTokenChannel::writeAndClose(-1, token, &error));
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(PresentationTokenChannelTests)

#include "tst_presentation_token_channel.moc"

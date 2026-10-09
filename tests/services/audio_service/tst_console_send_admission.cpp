// SPDX-License-Identifier: GPL-3.0-or-later
#include "console_send_admission_p.h"
#include <QtTest>

using namespace QindaQt::Audio;

namespace
{
constexpr SendProducerIdentity kProducer{1, 42, 420};
QList<ObservedSendNode> pair(const SendModuleDeclaration &d)
{
    return {{101, 51, 501, 42, d.playbackName + QStringLiteral(".capture"), d.token,
             QStringLiteral("capture"), QStringLiteral("Stream/Input/Audio")},
            {102, 52, 502, 42, d.playbackName, d.token,
             QStringLiteral("playback"), QStringLiteral("Stream/Output/Audio")}};
}
}

class ConsoleSendAdmissionTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void requiresBothActualProducerRoles();
    void copiedNameTokenAndDuplicate();
    void serialAndObjectReplacement();
    void retirementNeverReacquires();
    void producerReplacementRevokes();
};

void ConsoleSendAdmissionTests::requiresBothActualProducerRoles()
{
    ConsoleSendAdmission a;
    QVERIFY(!a.reserve(QStringLiteral("send.test")));
    a.reset(kProducer);
    const auto d = a.reserve(QStringLiteral("send.test")); QVERIFY(d);
    auto nodes = pair(*d);
    a.observe(kProducer, {nodes.first()});
    QVERIFY(!a.playback(d->instance));
    a.observe(kProducer, nodes);
    QCOMPARE(a.playback(d->instance)->serial, quint64(102));
    QVERIFY(a.excludes(101, 501)); QVERIFY(a.excludes(102, 502));
    QVERIFY(!a.excludes(102, 999)); QVERIFY(!a.excludes(999, 502));
}

void ConsoleSendAdmissionTests::copiedNameTokenAndDuplicate()
{
    ConsoleSendAdmission a; a.reset(kProducer);
    const auto d = a.reserve(QStringLiteral("send.test")); QVERIFY(d);
    auto nodes = pair(*d);
    auto foreign = nodes.last(); foreign.serial = 201; foreign.boundId = 61;
    foreign.objectIdentity = 601; foreign.clientId = 99;
    nodes.append(foreign);
    a.observe(kProducer, nodes);
    QVERIFY(a.playback(d->instance)); QVERIFY(!a.excludes(201, 601));
    auto duplicate = nodes.at(1); duplicate.serial = 202; duplicate.boundId = 62;
    duplicate.objectIdentity = 602;
    nodes.append(duplicate);
    a.observe(kProducer, nodes);
    QVERIFY(!a.playback(d->instance));
    QCOMPARE(a.invalidInstances(), QList<quint64>{d->instance});
    QVERIFY(!a.excludes(202, 602));
}

void ConsoleSendAdmissionTests::serialAndObjectReplacement()
{
    for (const bool replaceSerial : {false, true}) {
        ConsoleSendAdmission a; a.reset(kProducer);
        const auto d = a.reserve(QStringLiteral("send.test")); QVERIFY(d);
        auto nodes = pair(*d); a.observe(kProducer, nodes);
        if (replaceSerial) nodes[1].serial = 202;
        else nodes[1].objectIdentity = 602;
        a.observe(kProducer, nodes);
        QVERIFY(!a.playback(d->instance)); QVERIFY(!a.excludes(202, 602));
        a.observe(kProducer, pair(*d));
        QVERIFY(!a.playback(d->instance));
    }
}

void ConsoleSendAdmissionTests::retirementNeverReacquires()
{
    ConsoleSendAdmission a; a.reset(kProducer);
    const auto d = a.reserve(QStringLiteral("send.test")); QVERIFY(d);
    const auto nodes = pair(*d); a.observe(kProducer, nodes);
    a.retire(d->instance);
    QVERIFY(!a.playback(d->instance)); QVERIFY(a.excludes(102, 502));
    a.observe(kProducer, {});
    QCOMPARE(a.size(), qsizetype(0));
    a.observe(kProducer, nodes);
    QVERIFY(!a.excludes(102, 502));
    const auto fresh = a.reserve(QStringLiteral("send.test")); QVERIFY(fresh);
    QVERIFY(fresh->instance != d->instance); QVERIFY(fresh->token != d->token);
    a.observe(kProducer, nodes); QVERIFY(!a.playback(fresh->instance));
}

void ConsoleSendAdmissionTests::producerReplacementRevokes()
{
    ConsoleSendAdmission a; a.reset(kProducer);
    const auto d = a.reserve(QStringLiteral("send.test")); QVERIFY(d);
    const auto nodes = pair(*d); a.observe(kProducer, nodes);
    a.observe({2, 42, 420}, nodes);
    QCOMPARE(a.size(), qsizetype(0)); QVERIFY(!a.excludes(102, 502));
    a.reset({2, 42, 420}); QVERIFY(a.reserve(QStringLiteral("send.test")));
    a.revoke(); a.revoke(); QVERIFY(!a.reserve(QStringLiteral("send.test")));
}

QTEST_GUILESS_MAIN(ConsoleSendAdmissionTests)
#include "tst_console_send_admission.moc"

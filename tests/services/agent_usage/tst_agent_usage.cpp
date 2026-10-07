// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/agent_usage/agent_usage_collector.h>
#include "parsers.h"
#include <QTest>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
using namespace QindaQt::Services::AgentUsage;
class UsageTests : public QObject {
    Q_OBJECT
private slots:
    void emptyConstruction() {
        int clocks=0;
        AgentUsageCollector source("/does/not/exist","/does/not/exist",[&] {++clocks;return QDateTime::currentDateTimeUtc();});
        QCOMPARE(clocks,0);auto copy=source.snapshot();QCOMPARE(copy.size(),8);
        QCOMPARE(copy[0].state,UsageState::Unavailable);copy.clear();QCOMPARE(source.snapshot().size(),8);
    }
    void reportValidation() {
        auto now=QDateTime::fromString("2026-10-07T19:00:00Z",Qt::ISODate);
        QJsonObject report{{"schemaVersion",1},{"providerId","claude"},{"source","claude-statusline"},
            {"observedAt","2026-10-07T18:59:00Z"},{"scope","report-period"},{"tokenScope","context"},{"costScope","session"},
            {"inputTokens",50},{"reportedCostUsd",0.25}};
        auto parse=[&] {return Private::parseReport(QJsonDocument(report).toJson(),"claude",now);};
        auto good=parse();QCOMPARE(good.state,UsageState::Ready);QCOMPARE(good.tokenScope,"context");QCOMPARE(good.costScope,"session");
        QVERIFY(good.inputTokens==50);QVERIFY(!good.outputTokens);QVERIFY(!good.totalTokens);
        report["observedAt"]="2026-10-07T17:00:00Z";QCOMPARE(parse().state,UsageState::Stale);
        report["inputTokens"]=-1;auto bad=parse();QCOMPARE(bad.state,UsageState::Error);QVERIFY(!bad.reportedCostUsd);
        report["inputTokens"]=1;report["secret"]="must-not-leak";QCOMPARE(parse().state,UsageState::Error);
    }
    void codexBuckets() {
        ProviderUsage u;
        QJsonObject bucket{{"primary",QJsonObject{{"usedPercent",40},{"resetsAt",2000000000}}}};
        QVERIFY(Private::parseRateLimits({{"rateLimitsByLimitId",QJsonObject{{"a",bucket},{"b",bucket}}},{"rateLimits",bucket}},u));
        QCOMPARE(u.quotaWindows.size(),2);
        QVERIFY(Private::parseCodexUsage({{"summary",QJsonObject{{"lifetimeTokens",77}}}},u));
        QVERIFY(u.totalTokens==77);QVERIFY(!u.inputTokens);QVERIFY(!u.reportedCostUsd);QCOMPARE(u.tokenScope,"lifetime");
        ProviderUsage legacy;QVERIFY(Private::parseRateLimits({{"rateLimits",bucket}},legacy));QCOMPARE(legacy.quotaWindows.size(),1);
    }
    void processSingleFlight() {
        qputenv("QINDAQT_USAGE_FIXTURE","normal");QTemporaryDir dir;
        auto now=QDateTime::currentDateTimeUtc();
        AgentUsageCollector source(dir.path(),FIXTURE,[&] {return now;});
        QSignalSpy spy(&source,&AgentUsageSource::snapshotChanged);source.refresh();source.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(source.snapshot()[0].state==UsageState::Ready,2000);
        QVERIFY(source.snapshot()[0].totalTokens==1234);QCOMPARE(source.snapshot()[0].quotaWindows.size(),1);
        QCOMPARE(spy.count(),2);now=now.addSecs(901);QCOMPARE(source.snapshot()[0].state,UsageState::Stale);
    }
    void partialMetadata() {
        qputenv("QINDAQT_USAGE_FIXTURE","unsupported");
        AgentUsageCollector source("",FIXTURE,[] {return QDateTime::currentDateTimeUtc();});source.refresh();
        QTRY_COMPARE(source.snapshot()[0].state,UsageState::Ready);
        QCOMPARE(source.snapshot()[0].quotaWindows.size(),1);QVERIFY(!source.snapshot()[0].totalTokens);
        QVERIFY(!source.snapshot()[0].detail.contains("private"));
    }
    void hostileFeed() {
        QTemporaryDir dir;QFile f(dir.filePath("claude.json"));QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"schemaVersion\":1,\"schemaVersion\":1}");f.close();
        auto invalid=Private::parseReport("{\"schemaVersion\":1,\"schemaVersion\":1}","claude",QDateTime::currentDateTimeUtc());
        QCOMPARE(invalid.state,UsageState::Error);
        AgentUsageCollector source(dir.path(),"/missing",[] {return QDateTime::currentDateTimeUtc();});
        source.refresh();QCOMPARE(source.snapshot()[1].state,UsageState::Error);
        f.remove();QVERIFY(QFile::link("/etc/hostname",dir.filePath("claude.json")));
        QTRY_COMPARE(source.snapshot()[0].state,UsageState::Unavailable);
        source.refresh();QCOMPARE(source.snapshot()[1].state,UsageState::Error);
    }
    void processBounds_data() {
        QTest::addColumn<QByteArray>("mode");QTest::addColumn<int>("timeout");
        QTest::newRow("output-cap")<<QByteArray("overflow")<<2000;
        QTest::newRow("deadline")<<QByteArray("timeout")<<6500;
    }
    void processBounds() {
        QFETCH(QByteArray,mode);QFETCH(int,timeout);qputenv("QINDAQT_USAGE_FIXTURE",mode);
        AgentUsageCollector source("",FIXTURE,[] {return QDateTime::currentDateTimeUtc();});source.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(source.snapshot()[0].state==UsageState::Error,timeout);
        QVERIFY(!source.snapshot()[0].totalTokens);
    }
    void unavailableProgram() {
        AgentUsageCollector source("","/not-a-program",[] {return QDateTime::currentDateTimeUtc();});
        source.refresh();QTRY_COMPARE(source.snapshot()[0].state,UsageState::Unavailable);
    }
    void destructionBounded() {
        qputenv("QINDAQT_USAGE_FIXTURE","timeout");QElapsedTimer timer;timer.start();
        {AgentUsageCollector source("",FIXTURE,[] {return QDateTime::currentDateTimeUtc();});source.refresh();QTest::qWait(50);}
        QVERIFY(timer.elapsed()<1500);
    }
};
QTEST_GUILESS_MAIN(UsageTests)
#include "tst_agent_usage.moc"

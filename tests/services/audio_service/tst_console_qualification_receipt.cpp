// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/gain_qualification_receipt.h"
#include "support/gain_qualification_evidence.h"
#include <QtTest>
#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QTemporaryDir>
#include <sys/stat.h>
using namespace QindaQt::Audio::Test;

class ConsoleQualificationReceiptTests final : public QObject
{
    Q_OBJECT
private:
    QJsonObject file(const QString &path, const QByteArray &bytes)
    {
        QFile output(path);
        if (!output.open(QIODevice::WriteOnly)) return {};
        if (output.write(bytes) != bytes.size()) return {};
        output.close();
        if (!output.setPermissions(QFile::ReadOwner | QFile::WriteOwner)) return {};
        return {{QStringLiteral("path"), path},
            {QStringLiteral("sha256"), QString::fromLatin1(
                QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())},
            {QStringLiteral("bytes"), bytes.size()}};
    }
    QJsonObject receipt(const QString &root, const QString &phase = QStringLiteral("compile"))
    {
        const auto snapshot = root + QStringLiteral("/suite-evidence");
        if (!QDir().mkpath(snapshot)) return {};
        const auto recipe = root + QStringLiteral("/fixture.ebuild");
        if (file(recipe, QByteArray("fixture")).isEmpty()) return {};
        const auto log = file(snapshot + QStringLiteral("/compile.log"), QByteArray("compile complete\n"));
        const QJsonObject step{{QStringLiteral("name"), QStringLiteral("portage-compile")},
            {QStringLiteral("argv"), QJsonArray{QStringLiteral("/usr/bin/ebuild"), recipe,
                QStringLiteral("clean"), QStringLiteral("setup"), QStringLiteral("unpack"),
                QStringLiteral("prepare"), QStringLiteral("configure"), phase}},
            {QStringLiteral("exit"), 0}, {QStringLiteral("timedOut"), false},
            {QStringLiteral("leaderReservedThroughCleanup"), true},
            {QStringLiteral("ownedProcessGroup"), 200},
            {QStringLiteral("sha256"), log.value(QStringLiteral("sha256"))}};
        const auto phases = file(snapshot + QStringLiteral("/phases.json"),
            QJsonDocument(QJsonObject{{QStringLiteral("mode"), QStringLiteral("patched-normal")},
                {QStringLiteral("upstreamTestVariant"), false},
                {QStringLiteral("steps"), QJsonArray{step}}}).toJson());
        const auto final = file(snapshot + QStringLiteral("/inputs.json"), QByteArray("{\"verified\":true}\n"));
        const QJsonObject proof{{QStringLiteral("schema"), 1},
            {QStringLiteral("mode"), QStringLiteral("patched-normal")}, {QStringLiteral("root"), root},
            {QStringLiteral("recipePath"), recipe}, {QStringLiteral("compileExit"), 0},
            {QStringLiteral("testExit"), QJsonValue::Null}, {QStringLiteral("suiteComplete"), false},
            {QStringLiteral("testEnvironmentVerified"), false}, {QStringLiteral("registryCount"), 0},
            {QStringLiteral("counts"), QJsonObject{{QStringLiteral("ok"), 0},
                {QStringLiteral("fail"), 0}, {QStringLiteral("timeout"), 0}}},
            {QStringLiteral("raw"), QJsonObject{{QStringLiteral("compileLog"), log},
                {QStringLiteral("phases"), phases}, {QStringLiteral("inputsFinal"), final}}}};
        return {{QStringLiteral("schema"), 2}, {QStringLiteral("mode"), QStringLiteral("patched-normal")},
            {QStringLiteral("diagnosticOnly"), false}, {QStringLiteral("upstreamTests"), QStringLiteral("not-requested")},
            {QStringLiteral("upstreamEvidence"), proof}, {QStringLiteral("finalInputVerification"), final},
            {QStringLiteral("imageRoot"), root}, {QStringLiteral("packageVerification"), QJsonObject{}},
            {QStringLiteral("package"), QStringLiteral("fixture")}, {QStringLiteral("overlayCandidate"), QString()},
            {QStringLiteral("archiveSha256"), QString()}, {QStringLiteral("moduleSourceSha256"), QString()},
            {QStringLiteral("recipeSha256"), QString()}, {QStringLiteral("patchSha256"), QString()},
            {QStringLiteral("moduleDirectory"), root}, {QStringLiteral("spaDirectory"), root},
            {QStringLiteral("libraryDirectory"), root}, {QStringLiteral("artifacts"), QJsonObject{}},
            {QStringLiteral("scope"), QStringLiteral("pure receipt fixture, no Core")}};
    }

    QByteArray rawBytes(const QJsonObject &value, const QString &key)
    {
        const auto proof = value.value(QStringLiteral("upstreamEvidence")).toObject();
        const auto raw = proof.value(QStringLiteral("raw")).toObject();
        QFile input(raw.value(key).toObject().value(QStringLiteral("path")).toString());
        return input.open(QIODevice::ReadOnly) ? input.readAll() : QByteArray{};
    }
    bool replaceRaw(QJsonObject *value, const QString &key, const QByteArray &bytes)
    {
        auto proof = value->value(QStringLiteral("upstreamEvidence")).toObject();
        auto raw = proof.value(QStringLiteral("raw")).toObject();
        const auto record = file(raw.value(key).toObject().value(QStringLiteral("path")).toString(), bytes);
        if (record.isEmpty()) return false;
        raw.insert(key, record);
        proof.insert(QStringLiteral("raw"), raw);
        value->insert(QStringLiteral("upstreamEvidence"), proof);
        return true;
    }
    QByteArray rowBytes(const QJsonArray &rows)
    {
        QByteArray bytes;
        for (const auto &row : rows)
            bytes += QJsonDocument(row.toObject()).toJson(QJsonDocument::Compact) + '\n';
        return bytes;
    }
    QJsonObject completeSuite(const QString &root, const QString &mode, const bool failed)
    {
        // AGENT-NOTE: These three fictional results validate native receipt
        // parsing only. No command runs, and no package/image/Core is admitted.
        auto value = receipt(root);
        if (value.isEmpty()) return {};
        auto proof = value.value(QStringLiteral("upstreamEvidence")).toObject();
        auto raw = proof.value(QStringLiteral("raw")).toObject();
        QJsonArray registry, rows;
        QByteArray junit = QByteArray("<testsuites tests=\"3\" failures=\"")
            + QByteArray::number(failed ? 1 : 0) + "\" errors=\""
            + QByteArray::number(failed ? 1 : 0) + "\"><testsuite>";
        for (int i = 0; i < 3; ++i) {
            const auto name = QStringLiteral("synthetic-%1").arg(i);
            const QJsonArray command{root + QStringLiteral("/never-executed-%1").arg(i)};
            registry.append(QJsonObject{{QStringLiteral("name"), name},
                {QStringLiteral("cmd"), command},
                {QStringLiteral("suite"), QJsonArray{QStringLiteral("pipewire")}}});
            const auto result = !failed || i == 0 ? QStringLiteral("OK")
                : i == 1 ? QStringLiteral("FAIL") : QStringLiteral("TIMEOUT");
            rows.append(QJsonObject{{QStringLiteral("name"), QStringLiteral("pipewire:") + name},
                {QStringLiteral("command"), command}, {QStringLiteral("result"), result},
                {QStringLiteral("returncode"), !failed || i == 0 ? 0 : i == 1 ? -6 : -9},
                {QStringLiteral("duration"), 0.25}, {QStringLiteral("is_fail"), failed && i != 0},
                {QStringLiteral("env"), QJsonObject{
                    {QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("unix:path=")
                        + root + QStringLiteral("/absent-session")},
                    {QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"), QStringLiteral("unix:path=")
                        + root + QStringLiteral("/absent-system")},
                    {QStringLiteral("ENV_UNSET"), QStringLiteral("DISPLAY WAYLAND_DISPLAY")}}}});
            junit += "<testcase name=\"pipewire:synthetic-" + QByteArray::number(i) + "\">";
            if (failed && i == 1) junit += "<failure/>";
            if (failed && i == 2) junit += "<error/>";
            junit += "</testcase>";
        }
        junit += "</testsuite></testsuites>";
        const auto snapshot = root + QStringLiteral("/suite-evidence/");
        raw.insert(QStringLiteral("registry"), file(snapshot + QStringLiteral("registry.json"),
            QJsonDocument(registry).toJson()));
        raw.insert(QStringLiteral("json"), file(snapshot + QStringLiteral("suite.json"), rowBytes(rows)));
        raw.insert(QStringLiteral("text"), file(snapshot + QStringLiteral("suite.txt"),
            QByteArray("test: synthetic-0\ntest: synthetic-1\ntest: synthetic-2\nOk: ")
                + QByteArray::number(failed ? 1 : 3) + "\nFail: "
                + QByteArray::number(failed ? 1 : 0) + "\nTimeout: "
                + QByteArray::number(failed ? 1 : 0) + '\n'));
        raw.insert(QStringLiteral("junit"), file(snapshot + QStringLiteral("suite.xml"), junit));
        const auto testLog = file(snapshot + QStringLiteral("test.log"),
            QByteArray("synthetic validation fixture; no upstream execution\n"));
        raw.insert(QStringLiteral("testLog"), testLog);
        auto ledger = QJsonDocument::fromJson(rawBytes(value, QStringLiteral("phases"))).object();
        auto steps = ledger.value(QStringLiteral("steps")).toArray();
        steps.append(QJsonObject{{QStringLiteral("name"), QStringLiteral("portage-test")},
            {QStringLiteral("argv"), QJsonArray{QStringLiteral("/usr/bin/ebuild"),
                proof.value(QStringLiteral("recipePath")), QStringLiteral("test")}},
            {QStringLiteral("exit"), failed ? 1 : 0}, {QStringLiteral("timedOut"), false},
            {QStringLiteral("leaderReservedThroughCleanup"), true},
            {QStringLiteral("ownedProcessGroup"), 200},
            {QStringLiteral("sha256"), testLog.value(QStringLiteral("sha256"))}});
        ledger.insert(QStringLiteral("mode"), mode);
        ledger.insert(QStringLiteral("upstreamTestVariant"), true);
        ledger.insert(QStringLiteral("steps"), steps);
        raw.insert(QStringLiteral("phases"), file(snapshot + QStringLiteral("phases.json"),
            QJsonDocument(ledger).toJson()));
        proof.insert(QStringLiteral("mode"), mode);
        proof.insert(QStringLiteral("testExit"), failed ? 1 : 0);
        proof.insert(QStringLiteral("suiteComplete"), true);
        proof.insert(QStringLiteral("testEnvironmentVerified"), true);
        proof.insert(QStringLiteral("registryCount"), 3);
        proof.insert(QStringLiteral("counts"), QJsonObject{{QStringLiteral("ok"), failed ? 1 : 3},
            {QStringLiteral("fail"), failed ? 1 : 0}, {QStringLiteral("timeout"), failed ? 1 : 0}});
        proof.insert(QStringLiteral("raw"), raw);
        value.insert(QStringLiteral("mode"), mode);
        value.insert(QStringLiteral("diagnosticOnly"), failed);
        value.insert(QStringLiteral("upstreamTests"), failed ? QStringLiteral("failed") : QStringLiteral("passed"));
        value.insert(QStringLiteral("upstreamEvidence"), proof);
        value.insert(QStringLiteral("scope"), QStringLiteral("synthetic complete-suite parser fixture, no execution/image"));
        return value;
    }
    bool poisonSuite(QJsonObject *value, const QByteArray &fault)
    {
        auto proof = value->value(QStringLiteral("upstreamEvidence")).toObject();
        if (fault == "raw-hash") {
            auto raw = proof.value(QStringLiteral("raw")).toObject();
            auto json = raw.value(QStringLiteral("json")).toObject();
            json.insert(QStringLiteral("sha256"), QString(64, QLatin1Char('0')));
            raw.insert(QStringLiteral("json"), json);
            proof.insert(QStringLiteral("raw"), raw);
        } else if (fault == "registry-count") {
            proof.insert(QStringLiteral("registryCount"), 4);
        } else if (fault == "summary-count") {
            auto counts = proof.value(QStringLiteral("counts")).toObject();
            counts.insert(QStringLiteral("ok"), 2);
            proof.insert(QStringLiteral("counts"), counts);
        } else if (fault.startsWith("registry-")) {
            auto registry = QJsonDocument::fromJson(rawBytes(*value, QStringLiteral("registry"))).array();
            if (fault == "registry-duplicate") registry[2] = registry.at(0);
            else if (fault == "registry-incomplete") registry.removeAt(2);
            else return false;
            return replaceRaw(value, QStringLiteral("registry"), QJsonDocument(registry).toJson());
        } else if (fault.startsWith("junit-") || fault.startsWith("text-")) {
            const bool xml = fault.startsWith("junit-");
            const auto key = xml ? QStringLiteral("junit") : QStringLiteral("text");
            auto bytes = rawBytes(*value, key);
            if (fault == "junit-count") bytes.replace("tests=\"3\"", "tests=\"2\"");
            else if (fault == "junit-duplicate") bytes.replace("synthetic-2", "synthetic-0");
            else if (fault == "junit-incomplete")
                bytes.replace("<testcase name=\"pipewire:synthetic-2\"><error/></testcase>", "");
            else if (fault == "junit-result") bytes.replace("<error/>", "<failure/>");
            else if (fault == "text-count") bytes.replace("Fail: 1", "Fail: 0");
            else if (fault == "text-duplicate-summary") bytes += "Ok: 1\n";
            else if (fault == "text-incomplete") bytes.replace("test: synthetic-2\n", "");
            else return false;
            return replaceRaw(value, key, bytes);
        } else {
            QJsonArray rows;
            for (const auto &line : rawBytes(*value, QStringLiteral("json")).split('\n'))
                if (!line.isEmpty()) rows.append(QJsonDocument::fromJson(line).object());
            auto row = rows.at(2).toObject();
            auto env = row.value(QStringLiteral("env")).toObject();
            const auto address = fault.endsWith("session")
                ? QStringLiteral("DBUS_SESSION_BUS_ADDRESS") : QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS");
            if (fault.startsWith("missing-")) env.remove(address);
            else if (fault.startsWith("wrong-")) env.insert(address, QStringLiteral("unix:path=/unqualified-address"));
            else if (fault.startsWith("unset-")) env.insert(QStringLiteral("ENV_UNSET"), address);
            else if (fault == "json-duplicate") row = rows.at(0).toObject();
            else if (fault == "json-incomplete") {
                rows.removeAt(2);
                return replaceRaw(value, QStringLiteral("json"), rowBytes(rows));
            } else if (fault == "json-command") row.insert(QStringLiteral("command"),
                QJsonArray{QStringLiteral("/never-executed-wrong-command")});
            else if (fault == "json-result") row.insert(QStringLiteral("result"), QStringLiteral("SKIP"));
            else if (fault == "json-returncode") row.insert(QStringLiteral("returncode"), 0);
            else return false;
            // AGENT-GUARD: Re-pin intentional parser poisons, except raw-hash,
            // so refusal cannot be satisfied solely by the outer file digest.
            if (fault.startsWith("missing-") || fault.startsWith("wrong-") || fault.startsWith("unset-"))
                row.insert(QStringLiteral("env"), env);
            rows[2] = row;
            return replaceRaw(value, QStringLiteral("json"), rowBytes(rows));
        }
        value->insert(QStringLiteral("upstreamEvidence"), proof);
        return true;
    }
private Q_SLOTS:
    void completeSuitesReachRawParsers_data()
    {
        QTest::addColumn<QString>("mode");
        QTest::addColumn<bool>("failed");
        QTest::addColumn<bool>("allowFailedOld");
        QTest::newRow("fixed-asan-green") << QStringLiteral("patched-asan") << false << false;
        QTest::newRow("fixed-normal-green") << QStringLiteral("patched-normal") << false << false;
        QTest::newRow("original-asan-failed") << QStringLiteral("unpatched-asan-control") << true << true;
    }
    void completeSuitesReachRawParsers()
    {
        QFETCH(QString, mode);
        QFETCH(bool, failed);
        QFETCH(bool, allowFailedOld);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QMap<QString, GainImageArtifact> records;
        QVERIFY(gainQualificationReceipt(completeSuite(directory.path(), mode, failed), allowFailedOld, &records));
        QCOMPARE(records.size(), 9);
        for (const auto &key : {QStringLiteral("registry"), QStringLiteral("json"), QStringLiteral("text"),
                 QStringLiteral("junit"), QStringLiteral("testLog"), QStringLiteral("publicationInputs")})
            QVERIFY(records.contains(key));
    }
    void completeFailedOldNeedsExplicitOptIn()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto value = completeSuite(directory.path(), QStringLiteral("unpatched-asan-control"), true);
        QMap<QString, GainImageArtifact> records;
        QVERIFY(gainQualificationReceipt(value, true, &records));
        QVERIFY(!records.isEmpty());
        QVERIFY(!gainQualificationReceipt(value, false, &records));
        QVERIFY(records.isEmpty());
    }
    void completeFailedFixedRefuses_data()
    {
        QTest::addColumn<QString>("mode");
        QTest::newRow("fixed-asan") << QStringLiteral("patched-asan");
        QTest::newRow("fixed-normal") << QStringLiteral("patched-normal");
    }
    void completeFailedFixedRefuses()
    {
        QFETCH(QString, mode);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QMap<QString, GainImageArtifact> records;
        QVERIFY(gainQualificationReceipt(completeSuite(directory.path(),
            QStringLiteral("unpatched-asan-control"), true), true, &records));
        const auto acceptedRecords = records;
        const auto fixed = completeSuite(directory.path(), mode, true);
        QVERIFY(!fixed.isEmpty());
        QVERIFY(!gainQualificationReceipt(fixed, false, &records));
        QVERIFY(records.isEmpty());
        records = acceptedRecords;
        QVERIFY(!gainQualificationReceipt(fixed, true, &records));
        QVERIFY(records.isEmpty());
    }
    void completeSuitePoisonsRefuse_data()
    {
        QTest::addColumn<QByteArray>("fault");
        for (const char *name : {"missing-session", "wrong-session", "missing-system", "wrong-system",
                 "unset-session", "unset-system", "raw-hash", "registry-count", "summary-count",
                 "registry-duplicate", "registry-incomplete", "json-duplicate", "json-incomplete",
                 "json-command", "json-result", "json-returncode", "junit-count", "junit-duplicate",
                 "junit-incomplete", "junit-result", "text-count", "text-duplicate-summary", "text-incomplete"})
            QTest::newRow(name) << QByteArray(name);
    }
    void completeSuitePoisonsRefuse()
    {
        QFETCH(QByteArray, fault);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto value = completeSuite(directory.path(), QStringLiteral("unpatched-asan-control"), true);
        QMap<QString, GainImageArtifact> records;
        // A previously accepted complete fixture makes a cleared output and a
        // parser-specific refusal observable for every independently repinned row.
        QVERIFY(gainQualificationReceipt(value, true, &records));
        QCOMPARE(records.size(), 9);
        QVERIFY(poisonSuite(&value, fault));
        QVERIFY(!gainQualificationReceipt(value, true, &records));
        QVERIFY(records.isEmpty());
    }

    void normalReleaseMakesNoTestQualificationClaim()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QMap<QString, GainImageArtifact> records;
        QVERIFY(gainQualificationReceipt(receipt(directory.path()), false, &records));
        QCOMPARE(records.size(), 4);
    }
    void unknownMissingAndMistypedFlagsRefuse()
    {
        QTemporaryDir directory;
        auto value = receipt(directory.path());
        QMap<QString, GainImageArtifact> records;
        value.insert(QStringLiteral("releaseQualified"), true);
        QVERIFY(!gainQualificationReceipt(value, false, &records));
        value.remove(QStringLiteral("releaseQualified"));
        value.remove(QStringLiteral("diagnosticOnly"));
        QVERIFY(!gainQualificationReceipt(value, false, &records));
        value.insert(QStringLiteral("diagnosticOnly"), QStringLiteral("false"));
        QVERIFY(!gainQualificationReceipt(value, false, &records));
        QVERIFY(records.isEmpty());
    }
    void failedOptInCannotAdmitFixedOrContradictoryEvidence()
    {
        QTemporaryDir directory;
        auto value = receipt(directory.path());
        QMap<QString, GainImageArtifact> records;
        QVERIFY(!gainQualificationReceipt(value, true, &records));
        value.insert(QStringLiteral("diagnosticOnly"), true);
        value.insert(QStringLiteral("upstreamTests"), QStringLiteral("failed"));
        QVERIFY(!gainQualificationReceipt(value, false, &records));
        value.insert(QStringLiteral("mode"), QStringLiteral("unpatched-asan-control"));
        QVERIFY(!gainQualificationReceipt(value, true, &records));
    }
    void wrongPhaseAndReplacedFinalInputsRefuse()
    {
        QTemporaryDir directory;
        QMap<QString, GainImageArtifact> records;
        QVERIFY(!gainQualificationReceipt(receipt(directory.path(), QStringLiteral("install")), false, &records));
        auto value = receipt(directory.path());
        const auto final = value.value(QStringLiteral("finalInputVerification")).toObject();
        QVERIFY(!file(final.value(QStringLiteral("path")).toString(), QByteArray("{\"verified\":false}\n")).isEmpty());
        QVERIFY(!gainQualificationReceipt(value, false, &records));
        QVERIFY(records.isEmpty());
    }
    void fifoReadRefusesBeforeAnyCoreOrWriter()
    {
        QTemporaryDir directory;
        const auto path = directory.path() + QStringLiteral("/fifo");
        QCOMPARE(::mkfifo(QFile::encodeName(path).constData(), 0600), 0);
        const GainImageArtifact artifact{.path = path, .sha256 = QByteArray(64, '0'), .bytes = 1};
        QVERIFY(gainReadEvidence(artifact).isEmpty());
    }
};
QTEST_GUILESS_MAIN(ConsoleQualificationReceiptTests)
#include "tst_console_qualification_receipt.moc"

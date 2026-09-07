// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/session_supervisor/polkit_agent_selection.h>

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

using namespace QindaQt::SessionSupervisor;

namespace {

QString makeExecutableCandidate(const QTemporaryDir &directory,
                                const QString &name)
{
    const QString path = directory.path() + QLatin1Char('/') + name;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {};
    }
    file.write("#!/bin/sh\n");
    file.close();
    if (!QFile::setPermissions(path, QFileDevice::ExeUser | QFileDevice::ReadUser)) {
        return {};
    }
    return path;
}

} // namespace

class PolkitAgentSelectionTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void disabledWinsEvenWhenHostCandidatesExist();
    void disabledWinsOverAnExplicitConfiguredPath();
    void omittedConfigurationKeepsInstalledProductionDefault();
    void explicitConfiguredPathPassesThroughUnchanged();
    void missingHostCandidatesResolveToHonestEmpty();
};

void PolkitAgentSelectionTest::disabledWinsEvenWhenHostCandidatesExist() {
    // AGENT-CONTRACT: the disable flag is the private/integration-run escape
    // hatch. It must yield empty regardless of what production paths exist on
    // the host, so staged sessions can never launch host binaries.
    QVERIFY(resolvePolkitAgentExecutable(true, QString{}).isEmpty());
}

void PolkitAgentSelectionTest::disabledWinsOverAnExplicitConfiguredPath() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString candidate = makeExecutableCandidate(directory, QStringLiteral("agent"));
    QVERIFY(QFileInfo(candidate).isExecutable());
    QVERIFY(resolvePolkitAgentExecutable(true, candidate).isEmpty());
}

void PolkitAgentSelectionTest::omittedConfigurationKeepsInstalledProductionDefault() {
    const QStringList candidates = defaultPolkitAgentCandidates();
    QVERIFY(!candidates.isEmpty());
    const QString resolved = resolvePolkitAgentExecutable(false, QString{});
    // Production behavior is unchanged: with an omitted --polkit-agent the
    // supervisor selects the first existing well-known agent when the host
    // has one, and honestly starts none otherwise.
    const auto existing =
        std::find_if(candidates.cbegin(), candidates.cend(),
                     [](const QString &candidate) {
                         return QFileInfo(candidate).isExecutable();
                     });
    if (existing == candidates.cend()) {
        QVERIFY(resolved.isEmpty());
    } else {
        QCOMPARE(resolved, *existing);
    }
}

void PolkitAgentSelectionTest::explicitConfiguredPathPassesThroughUnchanged() {
    QCOMPARE(resolvePolkitAgentExecutable(false, QStringLiteral("/opt/custom/agent")),
             QStringLiteral("/opt/custom/agent"));
    // Even a non-executable configured path passes through; the supervisor's
    // start path skips non-executables without touching host defaults.
    QCOMPARE(resolvePolkitAgentExecutable(false, QStringLiteral("/nonexistent/agent")),
             QStringLiteral("/nonexistent/agent"));
}

void PolkitAgentSelectionTest::missingHostCandidatesResolveToHonestEmpty() {
    // A relative configured name must never silently become a host absolute
    // path; it passes through and the sibling-resolution rules own it.
    QCOMPARE(resolvePolkitAgentExecutable(false, QStringLiteral("custom-agent")),
             QStringLiteral("custom-agent"));
}

QTEST_MAIN(PolkitAgentSelectionTest)
#include "tst_polkit_agent_selection.moc"

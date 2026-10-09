// SPDX-License-Identifier: GPL-3.0-or-later
#include "gain_qualification_receipt.h"
#include "gain_qualification_evidence.h"
#include "gain_upstream_suite.h"
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

namespace QindaQt::Audio::Test
{
namespace {
bool trueValue(const QJsonValue &value)
{
    return value.isBool() && value.toBool();
}
bool falseValue(const QJsonValue &value)
{
    return value.isBool() && !value.toBool();
}
QJsonObject objectOf(const QByteArray &bytes)
{
    return QJsonDocument::fromJson(bytes).object();
}
bool phaseMatches(const QJsonValue &value, const QString &recipe, const QString &name,
                  const QStringList &phases, const qint64 exit, const GainImageArtifact &log)
{
    const auto step = value.toObject();
    QJsonArray command{QStringLiteral("/usr/bin/ebuild"), recipe};
    for (const auto &phase : phases) command.append(phase);
    return value.isObject() && step.value(QStringLiteral("name")).toString() == name
        && step.value(QStringLiteral("argv")).toArray() == command
        && gainInteger(step.value(QStringLiteral("exit")), exit, exit)
        && falseValue(step.value(QStringLiteral("timedOut")))
        && trueValue(step.value(QStringLiteral("leaderReservedThroughCleanup")))
        && gainInteger(step.value(QStringLiteral("ownedProcessGroup")), 1, 2147483647)
        && step.value(QStringLiteral("sha256")).toString().toLatin1() == log.sha256;
}
} // namespace
bool gainQualificationReceipt(const QJsonObject &receipt, const bool allowFailedOld,
                              QMap<QString, GainImageArtifact> *evidence)
{
    evidence->clear();
    if (!gainExactKeys(receipt, {QStringLiteral("schema"), QStringLiteral("mode"),
        QStringLiteral("imageRoot"), QStringLiteral("packageVerification"), QStringLiteral("package"),
        QStringLiteral("overlayCandidate"), QStringLiteral("archiveSha256"), QStringLiteral("moduleSourceSha256"),
        QStringLiteral("recipeSha256"), QStringLiteral("patchSha256"), QStringLiteral("moduleDirectory"),
        QStringLiteral("spaDirectory"), QStringLiteral("libraryDirectory"), QStringLiteral("artifacts"),
        QStringLiteral("scope"), QStringLiteral("finalInputVerification"), QStringLiteral("diagnosticOnly"),
        QStringLiteral("upstreamTests"), QStringLiteral("upstreamEvidence")})
        || !gainInteger(receipt.value(QStringLiteral("schema")), 2, 2))
        return false;
    const auto mode = receipt.value(QStringLiteral("mode")).toString();
    const bool old = mode == QLatin1String("unpatched-asan-control");
    const auto diagnostic = receipt.value(QStringLiteral("diagnosticOnly"));
    const auto status = receipt.value(QStringLiteral("upstreamTests")).toString();
    const bool failed = status == QLatin1String("failed");
    const bool tested = status != QLatin1String("not-requested");
    if ((!old && mode != QLatin1String("patched-asan") && mode != QLatin1String("patched-normal"))
        || !diagnostic.isBool() || diagnostic.toBool() != failed
        || (status != QLatin1String("passed") && !failed && status != QLatin1String("not-requested"))
        || (allowFailedOld && !old) || (failed && (!old || !allowFailedOld))
        || (!tested && mode != QLatin1String("patched-normal")))
        return false;
    const auto proof = receipt.value(QStringLiteral("upstreamEvidence")).toObject();
    if (!receipt.value(QStringLiteral("upstreamEvidence")).isObject()
        || !gainExactKeys(proof, {QStringLiteral("schema"), QStringLiteral("mode"), QStringLiteral("root"),
            QStringLiteral("recipePath"), QStringLiteral("compileExit"), QStringLiteral("testExit"),
            QStringLiteral("suiteComplete"), QStringLiteral("testEnvironmentVerified"),
            QStringLiteral("registryCount"), QStringLiteral("counts"), QStringLiteral("raw")})
        || !gainInteger(proof.value(QStringLiteral("schema")), 1, 1)
        || proof.value(QStringLiteral("mode")).toString() != mode
        || !gainInteger(proof.value(QStringLiteral("compileExit")), 0, 0))
        return false;
    const QString root = proof.value(QStringLiteral("root")).toString();
    const QString recipe = proof.value(QStringLiteral("recipePath")).toString();
    if (root.isEmpty() || root == QLatin1String("/") || root.size() > 4096
        || QFileInfo(root).canonicalFilePath() != root
        || recipe.isEmpty() || recipe.size() > 4096 || QFileInfo(recipe).canonicalFilePath() != recipe)
        return false;
    const auto raw = proof.value(QStringLiteral("raw")).toObject();
    QSet<QString> rawKeys{QStringLiteral("compileLog"), QStringLiteral("phases"), QStringLiteral("inputsFinal")};
    if (tested) rawKeys.unite({QStringLiteral("registry"), QStringLiteral("json"), QStringLiteral("text"),
                              QStringLiteral("junit"), QStringLiteral("testLog")});
    if (!proof.value(QStringLiteral("raw")).isObject() || !gainExactKeys(raw, rawKeys)) return false;
    QMap<QString, GainImageArtifact> records;
    QMap<QString, QByteArray> payloads;
    const auto snapshotRoot = root + QStringLiteral("/suite-evidence");
    for (auto it = raw.constBegin(); it != raw.constEnd(); ++it) {
        GainImageArtifact artifact;
        QByteArray bytes;
        if (!gainEvidenceRecord(it.value(), snapshotRoot, &artifact, &bytes)) return false;
        records.insert(it.key(), artifact);
        payloads.insert(it.key(), bytes);
    }
    if (!trueValue(objectOf(payloads.value(QStringLiteral("inputsFinal"))).value(QStringLiteral("verified"))))
        return false;
    GainImageArtifact finalInputs;
    QByteArray finalBytes;
    if (!gainEvidenceRecord(receipt.value(QStringLiteral("finalInputVerification")), snapshotRoot,
            &finalInputs, &finalBytes)
        || !trueValue(objectOf(finalBytes).value(QStringLiteral("verified"))))
        return false;
    records.insert(QStringLiteral("publicationInputs"), finalInputs);
    const auto ledger = objectOf(payloads.value(QStringLiteral("phases")));
    const auto steps = ledger.value(QStringLiteral("steps")).toArray();
    const auto variant = ledger.value(QStringLiteral("upstreamTestVariant"));
    if (ledger.value(QStringLiteral("mode")).toString() != mode || !variant.isBool()
        || variant.toBool() != tested || !ledger.value(QStringLiteral("steps")).isArray()
        || steps.size() != (tested ? 2 : 1)
        || !phaseMatches(steps.at(0), recipe, QStringLiteral("portage-compile"),
            {QStringLiteral("clean"), QStringLiteral("setup"), QStringLiteral("unpack"),
             QStringLiteral("prepare"), QStringLiteral("configure"), QStringLiteral("compile")},
            0, records.value(QStringLiteral("compileLog"))))
        return false;
    if (tested && !phaseMatches(steps.at(1), recipe, QStringLiteral("portage-test"),
            {QStringLiteral("test")}, failed ? 1 : 0, records.value(QStringLiteral("testLog"))))
        return false;
    const auto counts = proof.value(QStringLiteral("counts")).toObject();
    if (!proof.value(QStringLiteral("counts")).isObject()
        || !gainExactKeys(counts, {QStringLiteral("ok"), QStringLiteral("fail"), QStringLiteral("timeout")}))
        return false;
    for (const auto &key : counts.keys())
        if (!gainInteger(counts.value(key), 0, 256)) return false;
    const auto count = proof.value(QStringLiteral("registryCount"));
    if (tested) {
        if (!gainInteger(count, 1, 256)
            || !gainInteger(proof.value(QStringLiteral("testExit")), failed ? 1 : 0, failed ? 1 : 0)
            || !trueValue(proof.value(QStringLiteral("suiteComplete")))
            || !trueValue(proof.value(QStringLiteral("testEnvironmentVerified")))
            || !gainCompleteUpstreamSuite(payloads, root, counts, count.toInteger(), failed))
            return false;
    } else if (!gainInteger(count, 0, 0) || !proof.value(QStringLiteral("testExit")).isNull()
        || !falseValue(proof.value(QStringLiteral("suiteComplete")))
        || !falseValue(proof.value(QStringLiteral("testEnvironmentVerified")))
        || counts.value(QStringLiteral("ok")).toInteger() != 0
        || counts.value(QStringLiteral("fail")).toInteger() != 0
        || counts.value(QStringLiteral("timeout")).toInteger() != 0) {
        return false;
    }
    *evidence = records;
    return true;
}
} // namespace QindaQt::Audio::Test

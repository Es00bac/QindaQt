// SPDX-License-Identifier: GPL-3.0-or-later
#include "gain_qualified_image.h"
#include "gain_qualification_receipt.h"
#include "gain_qualification_evidence.h"
#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <pipewire/pipewire.h>
#include <wp/wp.h>

#ifndef QINDAQT_GAIN_COHORT_RECEIPT
#define QINDAQT_GAIN_COHORT_RECEIPT ""
#endif
#ifndef QINDAQT_GAIN_COHORT_RECEIPT_SHA256
#define QINDAQT_GAIN_COHORT_RECEIPT_SHA256 ""
#endif
#ifndef QINDAQT_GAIN_ALLOW_UNPATCHED_ASAN_CONTROL
#define QINDAQT_GAIN_ALLOW_UNPATCHED_ASAN_CONTROL 0
#endif
#ifndef QINDAQT_GAIN_ALLOW_FAILED_UPSTREAM_OLD_ASAN
#define QINDAQT_GAIN_ALLOW_FAILED_UPSTREAM_OLD_ASAN 0
#endif

namespace QindaQt::Audio::Test
{
namespace {
bool refuse(QString *reason, const char *token)
{
    if (reason) *reason = QString::fromLatin1(token);
    return false;
}
bool digest(const QByteArray &value)
{
    static const QRegularExpression shape(QStringLiteral("^[0-9a-f]{64}$"));
    return shape.match(QString::fromLatin1(value)).hasMatch();
}
} // namespace

bool QualifiedGainImage::preflight(QString *reason)
{
    m_ready = false;
    m_artifacts.clear();
    m_evidence.clear();
    m_unpatched = false;
    const QString receiptPath = QStringLiteral(QINDAQT_GAIN_COHORT_RECEIPT);
    const QByteArray pinned(QINDAQT_GAIN_COHORT_RECEIPT_SHA256);
    if (receiptPath.isEmpty() || !digest(pinned))
        return refuse(reason, "unqualified-loopback-cohort");
    GainImageArtifact receiptArtifact{.path = receiptPath, .sha256 = pinned,
        .bytes = QFileInfo(receiptPath).size()};
    const auto bytes = gainReadEvidence(receiptArtifact, 64 * 1024);
    if (bytes.isEmpty())
        return refuse(reason, QFileInfo(receiptPath).exists()
            ? "qualified-receipt-changed" : "qualified-receipt-unavailable");
    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return refuse(reason, "qualified-receipt-malformed");
    const auto object = document.object();
    const auto mode = object.value(QStringLiteral("mode")).toString();
    m_unpatched = mode == QLatin1String("unpatched-asan-control");
    if (!gainQualificationReceipt(object, QINDAQT_GAIN_ALLOW_FAILED_UPSTREAM_OLD_ASAN, &m_evidence)
        || object.value(QStringLiteral("archiveSha256")).toString()
            != QLatin1String("efbaedf80450e5b08bb3196a4ca7ea17850d4a71ec1a8ae321e8937a7c95779e")
        || (!m_unpatched && mode != QLatin1String("patched-asan")
                         && mode != QLatin1String("patched-normal"))
        || (m_unpatched && !QINDAQT_GAIN_ALLOW_UNPATCHED_ASAN_CONTROL)
        || object.value(QStringLiteral("moduleSourceSha256")).toString()
            != (m_unpatched
                ? QLatin1String("8b35c47c2d94707f7cba0efea40088e8f12bf24b3cfb973c2137eff80872fbe3")
                : QLatin1String("8a348844f3df801078fc0a0cb27f08231890a6db862084cfc8529b5667eed7ac"))
        || object.value(QStringLiteral("recipeSha256")).toString()
            != (m_unpatched
                ? QLatin1String("bacba65b91d111195d332f3f4e755ad9a747f19a712bbc7159f6430dc849a5e0")
                : QLatin1String("c9af68ec33b7c8ff13ac519be2b2629b87e271bda0bb0a30f2660b58789f9c62"))
        || (!m_unpatched && object.value(QStringLiteral("overlayCandidate")).toString()
            != QLatin1String("b489a5f8ce215da9fad5f5f10db0adbfa22af2a4"))
        || (!m_unpatched && object.value(QStringLiteral("patchSha256")).toString()
            != QLatin1String("23625878bbb2f7346f6eed7dec6a907435a178aa770b796e6870aaafd1528e22")))
        return refuse(reason, "qualified-source-cohort-refused");
    m_evidence.insert(QStringLiteral("receipt"), receiptArtifact);
    const auto proof = object.value(QStringLiteral("packageVerification")).toObject();
    if (!proof.value(QStringLiteral("fullPortageVerification")).toBool()
        || proof.value(QStringLiteral("matchedEntries")).toInteger() <= 0
        || !digest(proof.value(QStringLiteral("packageSha256")).toString().toLatin1())
        || !digest(proof.value(QStringLiteral("payloadManifestSha256")).toString().toLatin1())
        || (mode == QLatin1String("patched-normal")
            && !proof.value(QStringLiteral("requiredSignature")).toBool()))
        return refuse(reason, "qualified-package-image-refused");
    const auto artifacts = object.value(QStringLiteral("artifacts")).toObject();
    const QStringList required = {QStringLiteral("core"), QStringLiteral("module"),
        QStringLiteral("daemon"), QStringLiteral("policy"), QStringLiteral("serverConfig"),
        QStringLiteral("clientConfig"), QStringLiteral("policyConfig")};
    for (const auto &key : required) {
        const auto value = artifacts.value(key).toObject();
        GainImageArtifact artifact{.path = value.value(QStringLiteral("path")).toString(),
            .sha256 = value.value(QStringLiteral("sha256")).toString().toLatin1(),
            .bytes = value.value(QStringLiteral("bytes")).toInteger(-1)};
        if (!digest(artifact.sha256) || !exactGainImageFile(artifact))
            return refuse(reason, "qualified-artifact-refused");
        m_artifacts.insert(key, artifact);
    }
    const auto sanitizer = artifacts.value(QStringLiteral("asanRuntime")).toObject();
    if (mode.endsWith(QLatin1String("asan")) || m_unpatched) {
        GainImageArtifact artifact{.path = sanitizer.value(QStringLiteral("path")).toString(),
            .sha256 = sanitizer.value(QStringLiteral("sha256")).toString().toLatin1(),
            .bytes = sanitizer.value(QStringLiteral("bytes")).toInteger(-1)};
        if (!digest(artifact.sha256) || !exactGainImageFile(artifact))
            return refuse(reason, "qualified-sanitizer-refused");
        m_artifacts.insert(QStringLiteral("asanRuntime"), artifact);
    }
    m_moduleDirectory = object.value(QStringLiteral("moduleDirectory")).toString();
    m_spaDirectory = object.value(QStringLiteral("spaDirectory")).toString();
    m_libraryDirectory = object.value(QStringLiteral("libraryDirectory")).toString();
    if (m_moduleDirectory.isEmpty() || m_spaDirectory.isEmpty() || m_libraryDirectory.isEmpty()
        || QFileInfo(m_moduleDirectory).canonicalFilePath() != m_moduleDirectory
        || QFileInfo(m_spaDirectory).canonicalFilePath() != m_spaDirectory
        || QFileInfo(m_libraryDirectory).canonicalFilePath() != m_libraryDirectory
        || m_artifacts.value(QStringLiteral("module")).path
            != m_moduleDirectory + QStringLiteral("/libpipewire-module-loopback.so")
        || QFileInfo(m_artifacts.value(QStringLiteral("core")).path).absolutePath()
            != m_libraryDirectory)
        return refuse(reason, "qualified-module-directory-refused");
    const QString imageRoot = object.value(QStringLiteral("imageRoot")).toString();
    if (imageRoot.isEmpty() || imageRoot == QLatin1String("/")
        || QFileInfo(imageRoot).canonicalFilePath() != imageRoot)
        return refuse(reason, "qualified-image-root-refused");
    for (const auto &key : {QStringLiteral("core"), QStringLiteral("module"),
                           QStringLiteral("daemon"), QStringLiteral("serverConfig"),
                           QStringLiteral("clientConfig")})
        if (!m_artifacts.value(key).path.startsWith(imageRoot + QLatin1Char('/')))
            return refuse(reason, "qualified-image-path-refused");
    if (m_artifacts.value(QStringLiteral("daemon")).path
            != QFileInfo(QStringLiteral(QINDAQT_PIPEWIRE_EXECUTABLE)).canonicalFilePath()
        || m_artifacts.value(QStringLiteral("policy")).path
            != QFileInfo(QStringLiteral(QINDAQT_WIREPLUMBER_EXECUTABLE)).canonicalFilePath())
        return refuse(reason, "qualified-launcher-refused");
    m_ready = true;
    if (!bootstrapStillPinned() || !actualGainCoreImage(m_artifacts.value(QStringLiteral("core")))
        || QByteArray(pw_get_library_version()) != QByteArray("1.6.8")
        || QByteArray(wp_get_library_version()) != QByteArray("0.5.15")) {
        m_ready = false;
        return refuse(reason, "qualified-loader-cohort-refused");
    }
    return true;
}
QMap<QByteArray, QByteArray> QualifiedGainImage::bootstrapValues() const
{
    if (!m_ready) return {};
    return {{"PIPEWIRE_MODULE_DIR", m_moduleDirectory.toUtf8()},
        {"SPA_PLUGIN_DIR", m_spaDirectory.toUtf8()},
        {"LD_LIBRARY_PATH", m_libraryDirectory.toUtf8()},
        {"LD_PRELOAD", m_artifacts.value(QStringLiteral("asanRuntime")).path.toUtf8()}};
}
bool QualifiedGainImage::bootstrapStillPinned(const bool requireSpa) const
{
    if (!m_ready) return false;
    const auto values = bootstrapValues();
    if (requireSpa && qgetenv("SPA_PLUGIN_DIR") != values.value("SPA_PLUGIN_DIR")) return false;
    for (const auto &key : {QByteArray("PIPEWIRE_MODULE_DIR"),
                           QByteArray("LD_LIBRARY_PATH"), QByteArray("LD_PRELOAD")})
        if (qgetenv(key.constData()) != values.value(key)) return false;
    for (const auto &name : QProcessEnvironment::systemEnvironment().keys()) {
        if (name.startsWith(QLatin1String("LD_")) && name != QLatin1String("LD_LIBRARY_PATH")
            && name != QLatin1String("LD_PRELOAD"))
            return false;
    }
    for (const auto &artifact : m_evidence)
        if (gainReadEvidence(artifact).isEmpty()) return false;
    return exactGainImageFile(m_artifacts.value(QStringLiteral("module")))
        && actualGainCoreImage(m_artifacts.value(QStringLiteral("core")));
}
bool QualifiedGainImage::loadedModule(pw_impl_module *module) const
{
    return bootstrapStillPinned()
        && actualGainModuleImage(module, m_artifacts.value(QStringLiteral("module")));
}
bool QualifiedGainImage::privateChild(const qint64 pid) const
{
    return bootstrapStillPinned()
        && gainProcessMapsImage(pid, m_artifacts.value(QStringLiteral("core")));
}
} // namespace QindaQt::Audio::Test

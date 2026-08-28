// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwindevelopmentoutputseam.h"

#include <core/backendoutput.h>
#include <core/outputbackend.h>

#include <QJsonDocument>

#include <cmath>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

constexpr auto DisabledCode = "control-disabled";
constexpr auto DisabledMessage = "external compositor mutations are disabled";

QByteArray response(QString status,
                    QString code = {},
                    QString message = {},
                    const QString &name = {})
{
    QJsonObject object{{QStringLiteral("status"), std::move(status)}};
    if (!code.isEmpty()) {
        object.insert(QStringLiteral("failure"),
                      QJsonObject{{QStringLiteral("code"), std::move(code)},
                                  {QStringLiteral("message"), std::move(message)}});
    }
    if (!name.isEmpty()) {
        object.insert(QStringLiteral("name"), name);
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool validName(const QString &name)
{
    const auto utf8 = name.toUtf8();
    if (utf8.isEmpty()
        || utf8.size() > DevelopmentOutputController::MaxNameUtf8Bytes) {
        return false;
    }
    for (qsizetype index = 0; index < utf8.size(); ++index) {
        const char character = utf8[index];
        const bool alphaNumeric = (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9');
        const bool punctuation = index > 0
            && (character == '-' || character == '_' || character == '.');
        if (!alphaNumeric && !punctuation) {
            return false;
        }
    }
    return true;
}

QByteArray mutationFailure(DevelopmentOutputMutationResult result)
{
    if (result == DevelopmentOutputMutationResult::BackendUnavailable) {
        return response(QStringLiteral("rejected"),
                        QStringLiteral("virtual-output-unavailable"),
                        QStringLiteral("KWin output backend is unavailable"));
    }
    return response(QStringLiteral("rejected"),
                    QStringLiteral("virtual-output-backend-rejected"),
                    QStringLiteral("KWin rejected the virtual output mutation"));
}

} // namespace

DevelopmentOutputController::DevelopmentOutputController(
    bool mutationsEnabled,
    DevelopmentOutputMutator *mutator)
    : m_mutationsEnabled(mutationsEnabled)
    , m_mutator(mutator)
{
}

QByteArray DevelopmentOutputController::addVirtualOutputForTest(
    const QString &name,
    int width,
    int height,
    double scale) const
{
    // AGENT-GUARD: This typed method is visible on an unauthenticated user
    // bus. Production rejects before validating attacker-controlled strings or
    // numbers and before consulting KWin, preserving one inert response.
    if (!m_mutationsEnabled) {
        return response(QStringLiteral("rejected"), QString::fromLatin1(DisabledCode),
                        QString::fromLatin1(DisabledMessage));
    }
    if (!validName(name) || width < MinimumWidth || height < MinimumHeight
        || width > MaximumLogicalDimension || height > MaximumLogicalDimension
        || !std::isfinite(scale) || scale < MinimumScale || scale > MaximumScale) {
        return response(QStringLiteral("rejected"),
                        QStringLiteral("malformed-virtual-output-request"),
                        QStringLiteral("virtual output name, size, or scale is invalid"));
    }
    if (!m_mutator || !m_mutator->isAvailable()) {
        return mutationFailure(DevelopmentOutputMutationResult::BackendUnavailable);
    }
    if (m_mutator->ownedOutputCount() >= MaxOwnedVirtualOutputs
        || m_mutator->totalOutputCount() >= MaxTotalOutputs) {
        return response(QStringLiteral("rejected"),
                        QStringLiteral("virtual-output-limit"),
                        QStringLiteral("virtual output count exceeds the development limit"));
    }
    if (m_mutator->outputNameExists(name) || m_mutator->ownsOutput(name)) {
        return response(QStringLiteral("rejected"),
                        QStringLiteral("output-name-in-use"),
                        QStringLiteral("an output already uses the requested name"));
    }
    const auto result = m_mutator->addVirtualOutput(name, QSize(width, height), scale);
    if (result != DevelopmentOutputMutationResult::Succeeded) {
        return mutationFailure(result);
    }
    return response(QStringLiteral("added"), {}, {}, name);
}

QByteArray DevelopmentOutputController::removeVirtualOutputForTest(
    const QString &name) const
{
    if (!m_mutationsEnabled) {
        return response(QStringLiteral("rejected"), QString::fromLatin1(DisabledCode),
                        QString::fromLatin1(DisabledMessage));
    }
    if (!validName(name)) {
        return response(QStringLiteral("rejected"),
                        QStringLiteral("malformed-virtual-output-request"),
                        QStringLiteral("virtual output name is invalid"));
    }
    if (!m_mutator || !m_mutator->isAvailable()) {
        return mutationFailure(DevelopmentOutputMutationResult::BackendUnavailable);
    }
    if (!m_mutator->ownsOutput(name)) {
        return response(QStringLiteral("rejected"),
                        QStringLiteral("unknown-virtual-output"),
                        QStringLiteral("the virtual output is not owned by this plugin"));
    }
    const auto result = m_mutator->removeVirtualOutput(name);
    if (result != DevelopmentOutputMutationResult::Succeeded) {
        return mutationFailure(result);
    }
    return response(QStringLiteral("removed"), {}, {}, name);
}

QJsonObject DevelopmentOutputController::capabilities() const
{
    const bool available = m_mutationsEnabled && m_mutator
        && m_mutator->isAvailable();
    return {
        {QStringLiteral("enabled"), m_mutationsEnabled},
        {QStringLiteral("available"), available},
        {QStringLiteral("maxOwnedVirtualOutputs"),
         static_cast<qint64>(MaxOwnedVirtualOutputs)},
        {QStringLiteral("maxTotalOutputs"), static_cast<qint64>(MaxTotalOutputs)},
        {QStringLiteral("maxNameUtf8Bytes"), static_cast<qint64>(MaxNameUtf8Bytes)},
        {QStringLiteral("minimumWidth"), MinimumWidth},
        {QStringLiteral("minimumHeight"), MinimumHeight},
        {QStringLiteral("maximumLogicalDimension"), MaximumLogicalDimension},
        {QStringLiteral("minimumScale"), MinimumScale},
        {QStringLiteral("maximumScale"), MaximumScale},
    };
}

void DevelopmentOutputController::shutdown()
{
    if (m_mutator) {
        m_mutator->removeOwnedOutputs();
    }
    m_mutator = nullptr;
    m_mutationsEnabled = false;
}

KWinDevelopmentOutputSeam::KWinDevelopmentOutputSeam(
    KWin::OutputBackend *backend,
    QObject *parent)
    : QObject(parent)
    , m_backend(backend)
{
}

KWinDevelopmentOutputSeam::~KWinDevelopmentOutputSeam()
{
    removeOwnedOutputs();
}

bool KWinDevelopmentOutputSeam::isAvailable() const
{
    return !m_backend.isNull();
}

qsizetype KWinDevelopmentOutputSeam::totalOutputCount() const
{
    return m_backend ? m_backend->outputs().size() : 0;
}

qsizetype KWinDevelopmentOutputSeam::ownedOutputCount() const
{
    qsizetype count = 0;
    for (const auto &output : m_ownedOutputs) {
        if (output) {
            ++count;
        }
    }
    return count;
}

bool KWinDevelopmentOutputSeam::outputNameExists(const QString &name) const
{
    if (!m_backend) {
        return false;
    }
    for (const auto *output : m_backend->outputs()) {
        if (output && (output->name() == name
                       || output->name() == QStringLiteral("Virtual-%1").arg(name))) {
            return true;
        }
    }
    return false;
}

bool KWinDevelopmentOutputSeam::ownsOutput(const QString &name) const
{
    return !m_ownedOutputs.value(name).isNull();
}

DevelopmentOutputMutationResult KWinDevelopmentOutputSeam::addVirtualOutput(
    const QString &name,
    const QSize &logicalSize,
    qreal scale)
{
    if (!m_backend) {
        return DevelopmentOutputMutationResult::BackendUnavailable;
    }
    KWin::BackendOutput *const created = m_backend->createVirtualOutput(
        name,
        QStringLiteral("QindaQt development test output %1").arg(name),
        logicalSize,
        scale);
    if (!created || !m_backend->outputs().contains(created)) {
        return DevelopmentOutputMutationResult::BackendRejected;
    }
    m_ownedOutputs.insert(name, created);
    connect(created, &QObject::destroyed, this, [this, name, created] {
        if (m_ownedOutputs.value(name).data() == created) {
            m_ownedOutputs.remove(name);
        }
    });
    return DevelopmentOutputMutationResult::Succeeded;
}

DevelopmentOutputMutationResult KWinDevelopmentOutputSeam::removeVirtualOutput(
    const QString &name)
{
    if (!m_backend) {
        return DevelopmentOutputMutationResult::BackendUnavailable;
    }
    const auto output = m_ownedOutputs.value(name);
    if (!output) {
        m_ownedOutputs.remove(name);
        return DevelopmentOutputMutationResult::BackendRejected;
    }
    m_backend->removeVirtualOutput(output.data());
    if (output && m_backend->outputs().contains(output.data())) {
        return DevelopmentOutputMutationResult::BackendRejected;
    }
    m_ownedOutputs.remove(name);
    return DevelopmentOutputMutationResult::Succeeded;
}

void KWinDevelopmentOutputSeam::removeOwnedOutputs()
{
    const auto ownedOutputs = m_ownedOutputs;
    m_ownedOutputs.clear();
    if (!m_backend) {
        return;
    }
    for (const auto &output : ownedOutputs) {
        if (output && m_backend->outputs().contains(output.data())) {
            m_backend->removeVirtualOutput(output.data());
        }
    }
}

} // namespace QindaQt::Compositor::KWinIntegration

// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/design_tokens/token_facade.h"

#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"

#include <QCoreApplication>
#include <QThread>

#include <limits>
#include <utility>

namespace QindaQt::DesignTokens {
namespace {

QVariantMap nestedMap(const QVariantMap &all, const QString &name)
{
    return all.value(name).toMap();
}

} // namespace

TokenFacade::TokenFacade(QObject *parent)
    : QObject(parent)
{
}

bool TokenFacade::ready() const { return m_tokens != nullptr; }
int TokenFacade::qstRevision() const { return DesignTokens::qstRevision; }
qulonglong TokenFacade::generation() const { return m_generation; }
QString TokenFacade::sourceThemeId() const
{
    return m_tokens != nullptr ? m_tokens->sourceThemeId() : QString();
}
QVariantMap TokenFacade::bg() const { return nestedMap(m_all, QStringLiteral("bg")); }
QVariantMap TokenFacade::fg() const { return nestedMap(m_all, QStringLiteral("fg")); }
QVariantMap TokenFacade::accent() const { return nestedMap(m_all, QStringLiteral("accent")); }
QVariantMap TokenFacade::state() const { return nestedMap(m_all, QStringLiteral("state")); }
QVariantMap TokenFacade::focus() const { return nestedMap(m_all, QStringLiteral("focus")); }
QVariantMap TokenFacade::outline() const { return nestedMap(m_all, QStringLiteral("outline")); }
QVariantMap TokenFacade::status() const { return nestedMap(m_all, QStringLiteral("status")); }
QVariantMap TokenFacade::danger() const { return nestedMap(m_all, QStringLiteral("danger")); }
QVariantMap TokenFacade::radius() const { return nestedMap(m_all, QStringLiteral("radius")); }
QVariantMap TokenFacade::space() const { return nestedMap(m_all, QStringLiteral("space")); }
QVariantMap TokenFacade::type() const { return nestedMap(m_all, QStringLiteral("type")); }
QVariantMap TokenFacade::motion() const { return nestedMap(m_all, QStringLiteral("motion")); }
QVariantMap TokenFacade::elevation() const { return nestedMap(m_all, QStringLiteral("elevation")); }

bool TokenFacade::publish(const QindaQt::Themes::ThemeSpec &theme,
                          const AccessibilityInputs &inputs,
                          QString *error)
{
    if (!onOwningThread(error)) {
        return false;
    }
    const DerivationResult result = DesignTokenDeriver::derive(theme, inputs);
    if (!result.ok()) {
        if (error != nullptr) {
            *error = result.diagnostic;
        }
        return false;
    }
    return publish(result.tokens, error);
}

bool TokenFacade::publish(std::shared_ptr<const DesignTokens> tokens, QString *error)
{
    if (!onOwningThread(error)) {
        return false;
    }
    if (tokens == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("cannot publish null design tokens");
        }
        return false;
    }
    if (m_tokens != nullptr && *m_tokens == *tokens) {
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }

    m_tokens = std::move(tokens);
    rebuildMaps();
    if (m_generation < std::numeric_limits<qulonglong>::max()) {
        ++m_generation;
    }
    if (error != nullptr) {
        error->clear();
    }
    emit tokensChanged();
    return true;
}

bool TokenFacade::onOwningThread(QString *error) const
{
    const auto *application = QCoreApplication::instance();
    if (application != nullptr && thread() == application->thread()
        && thread() == QThread::currentThread()) {
        return true;
    }
    if (error != nullptr) {
        *error = QStringLiteral("QindaQt.Tokens publication requires the facade's GUI thread");
    }
    return false;
}

void TokenFacade::rebuildMaps()
{
    // AGENT-GUARD: Replace the complete cached map before one aggregate change
    // signal. Observers must never see roles from two token generations.
    m_all = m_tokens->toVariantMap();
}

} // namespace QindaQt::DesignTokens

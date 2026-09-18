// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/design_tokens/token_facade.h"

#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"

#include <QCoreApplication>
#include <QEvent>
#include <QInputDevice>
#include <QMouseEvent>
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
    // Touch mode (ADR-0193): a touchscreen among the seat's devices makes it
    // available; the last input kind decides whether it is active. The
    // application's events are the only observer a library singleton has.
    for (const QInputDevice *device : QInputDevice::devices()) {
        if (device != nullptr && device->type() == QInputDevice::DeviceType::TouchScreen) {
            m_touchAvailable = true;
            break;
        }
    }
    if (auto *application = QCoreApplication::instance()) {
        application->installEventFilter(this);
    }
    m_all.insert(QStringLiteral("touch"), touchMap());
}

TokenFacade::~TokenFacade()
{
    if (auto *application = QCoreApplication::instance()) {
        application->removeEventFilter(this);
    }
}

bool TokenFacade::eventFilter(QObject *watched, QEvent *event)
{
    if (event != nullptr) {
        switch (event->type()) {
        case QEvent::TouchBegin:
            setTouchState(true, true);
            break;
        case QEvent::MouseButtonPress:
        case QEvent::Wheel:
            // A synthesized mouse event is the finger again; a real one is a
            // pointer, and the desktop goes back to pointer sizes.
            if (const auto *input = dynamic_cast<const QInputEvent *>(event);
                input != nullptr && input->device() != nullptr
                && input->device()->type() != QInputDevice::DeviceType::TouchScreen
                && (event->type() != QEvent::MouseButtonPress
                    || static_cast<const QMouseEvent *>(event)->source()
                        == Qt::MouseEventNotSynthesized)) {
                setTouchState(m_touchAvailable, false);
            }
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}

QVariantMap TokenFacade::touchMap() const
{
    // The 44-logical-pixel target is the smallest a fingertip lands on
    // reliably; rows and gaps follow it. Off, every value is zero so a
    // control's own pointer size wins in Math.max.
    return {{QStringLiteral("available"), m_touchAvailable},
            {QStringLiteral("active"), m_touchActive},
            {QStringLiteral("minimumTarget"), m_touchActive ? 44.0 : 0.0},
            {QStringLiteral("rowHeight"), m_touchActive ? 48.0 : 0.0},
            {QStringLiteral("gap"), m_touchActive ? 8.0 : 0.0}};
}

QVariantMap TokenFacade::touch() const { return nestedMap(m_all, QStringLiteral("touch")); }

void TokenFacade::setTouchState(bool available, bool active)
{
    const bool nextActive = available && active;
    if (m_touchAvailable == available && m_touchActive == nextActive) {
        return;
    }
    m_touchAvailable = available;
    m_touchActive = nextActive;
    m_all.insert(QStringLiteral("touch"), touchMap());
    emit touchChanged();
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
QVariantMap TokenFacade::material() const { return nestedMap(m_all, QStringLiteral("material")); }
QVariantMap TokenFacade::accessibility() const
{
    return nestedMap(m_all, QStringLiteral("accessibility"));
}

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
    const auto &inputs = m_tokens->inputs();
    m_all.insert(QStringLiteral("accessibility"),
                 QVariantMap{{QStringLiteral("reducedMotion"), inputs.reducedMotion},
                             {QStringLiteral("reducedTransparency"),
                              inputs.reducedTransparency},
                             {QStringLiteral("highContrast"), inputs.highContrast}});
    m_all.insert(QStringLiteral("touch"), touchMap());
}

} // namespace QindaQt::DesignTokens

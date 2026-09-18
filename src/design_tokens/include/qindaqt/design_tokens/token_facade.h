// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/design_tokens/accessibility_inputs.h"

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace QindaQt::Themes {
class ThemeSpec;
}

namespace QindaQt::DesignTokens {

class DesignTokens;

// AGENT-CONTRACT: Each QQmlEngine owns its singleton for the engine lifetime.
// It is GUI-thread confined, has no persistence or service authority, and
// publishes one aggregate signal only after a complete immutable value swap.
// The QML surface is read-only; C++ composition owns publication and errors.
class TokenFacade final : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Tokens)
    QML_SINGLETON

    Q_PROPERTY(bool ready READ ready NOTIFY tokensChanged FINAL)
    Q_PROPERTY(int qstRevision READ qstRevision CONSTANT FINAL)
    Q_PROPERTY(qulonglong generation READ generation NOTIFY tokensChanged FINAL)
    Q_PROPERTY(QString sourceThemeId READ sourceThemeId NOTIFY tokensChanged FINAL)
    Q_PROPERTY(QVariantMap bg READ bg NOTIFY tokensChanged FINAL)
    Q_PROPERTY(QVariantMap fg READ fg NOTIFY tokensChanged FINAL)
    Q_PROPERTY(QVariantMap accent READ accent NOTIFY tokensChanged FINAL)
    Q_PROPERTY(QVariantMap state READ state NOTIFY tokensChanged FINAL)
    Q_PROPERTY(QVariantMap focus READ focus NOTIFY tokensChanged FINAL)
    Q_PROPERTY(QVariantMap outline READ outline NOTIFY tokensChanged FINAL)
    Q_PROPERTY(QVariantMap status READ status NOTIFY tokensChanged FINAL)
    Q_PROPERTY(QVariantMap danger READ danger NOTIFY tokensChanged FINAL)
    Q_PROPERTY(QVariantMap radius READ radius NOTIFY tokensChanged FINAL)
    Q_PROPERTY(QVariantMap space READ space NOTIFY tokensChanged FINAL)
    Q_PROPERTY(QVariantMap type READ type NOTIFY tokensChanged FINAL)
    Q_PROPERTY(QVariantMap motion READ motion NOTIFY tokensChanged FINAL)
    Q_PROPERTY(QVariantMap elevation READ elevation NOTIFY tokensChanged FINAL)
    Q_PROPERTY(QVariantMap accessibility READ accessibility NOTIFY tokensChanged FINAL)
    // Touch mode (ADR-0193): `available` when a touchscreen is present,
    // `active` while the last input was a finger, and the sizes controls
    // adopt then (`minimumTarget`, `rowHeight`, `gap`; zero otherwise).
    Q_PROPERTY(QVariantMap touch READ touch NOTIFY touchChanged FINAL)

public:
    explicit TokenFacade(QObject *parent = nullptr);
    ~TokenFacade() override;

    bool eventFilter(QObject *watched, QEvent *event) override;

    [[nodiscard]] bool ready() const;
    [[nodiscard]] int qstRevision() const;
    [[nodiscard]] qulonglong generation() const;
    [[nodiscard]] QString sourceThemeId() const;
    [[nodiscard]] QVariantMap bg() const;
    [[nodiscard]] QVariantMap fg() const;
    [[nodiscard]] QVariantMap accent() const;
    [[nodiscard]] QVariantMap state() const;
    [[nodiscard]] QVariantMap focus() const;
    [[nodiscard]] QVariantMap outline() const;
    [[nodiscard]] QVariantMap status() const;
    [[nodiscard]] QVariantMap danger() const;
    [[nodiscard]] QVariantMap radius() const;
    [[nodiscard]] QVariantMap space() const;
    [[nodiscard]] QVariantMap type() const;
    [[nodiscard]] QVariantMap motion() const;
    [[nodiscard]] QVariantMap elevation() const;
    [[nodiscard]] QVariantMap accessibility() const;
    [[nodiscard]] QVariantMap touch() const;
    // Sets the touch state directly (compositions and tests); the facade
    // otherwise observes the application's input devices and events itself.
    void setTouchState(bool available, bool active);
    [[nodiscard]] bool touchAvailable() const noexcept { return m_touchAvailable; }
    [[nodiscard]] bool touchActive() const noexcept { return m_touchActive; }

    // AGENT-CONTRACT: These are C++ composition APIs, intentionally not
    // Q_INVOKABLE. QML consumers can observe token generations but cannot
    // install themes, alter accessibility inputs, or mutate token maps.
    [[nodiscard]] bool publish(const QindaQt::Themes::ThemeSpec &theme,
                               const AccessibilityInputs &inputs,
                               QString *error = nullptr);
    [[nodiscard]] bool publish(std::shared_ptr<const DesignTokens> tokens,
                               QString *error = nullptr);

signals:
    void tokensChanged();
    // Touch mode alone (ADR-0193): a finger/pointer alternation must not
    // re-evaluate every colour, spacing and motion binding in the shell.
    void touchChanged();

private:
    [[nodiscard]] bool onOwningThread(QString *error) const;
    void rebuildMaps();

    std::shared_ptr<const DesignTokens> m_tokens;
    qulonglong m_generation = 0;
    [[nodiscard]] QVariantMap touchMap() const;

    QVariantMap m_all;
    bool m_touchAvailable = false;
    bool m_touchActive = false;
};

} // namespace QindaQt::DesignTokens

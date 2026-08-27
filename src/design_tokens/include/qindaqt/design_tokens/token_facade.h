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

public:
    explicit TokenFacade(QObject *parent = nullptr);

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

private:
    [[nodiscard]] bool onOwningThread(QString *error) const;
    void rebuildMaps();

    std::shared_ptr<const DesignTokens> m_tokens;
    qulonglong m_generation = 0;
    QVariantMap m_all;
};

} // namespace QindaQt::DesignTokens

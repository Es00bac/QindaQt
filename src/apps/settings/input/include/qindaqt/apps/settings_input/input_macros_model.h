// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "json_input_port.h"
#include <QVariantList>
namespace QindaQt::Apps::SettingsInput {
// GUI-thread projection of InputActions1. Source text stays in the owner's
// settings file; no command execution or recording happens in this model.
class InputMacrosModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY viewChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY viewChanged)
    Q_PROPERTY(QVariantList macros READ macros NOTIFY viewChanged)
    Q_PROPERTY(QVariantList actions READ actions CONSTANT)
    Q_PROPERTY(QString errorText READ errorText NOTIFY viewChanged)
    Q_PROPERTY(QString running READ running NOTIFY viewChanged)
public:
    explicit InputMacrosModel(JsonInputPort &port, QObject *parent = nullptr);
    bool available() const { return m_available; }
    bool busy() const { return m_busy; }
    QVariantList macros() const { return m_macros; }
    QVariantList actions() const;
    QString errorText() const { return m_error; }
    QString running() const { return m_running; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool save(const QVariantMap &macro, const QString &expected);
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE bool run(const QString &id);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE QVariantMap create(const QString &preset = {}) const;
    Q_INVOKABLE QString fingerprint(const QString &id) const;
    Q_INVOKABLE QString macroError(const QVariantMap &macro) const;
    Q_INVOKABLE QString stepError(const QVariantMap &step) const;
    Q_INVOKABLE QVariantMap combination(const QVariantMap &binding) const;
    Q_INVOKABLE QString bindingError(const QVariantMap &binding) const;
    Q_INVOKABLE QString stepSummary(const QVariantMap &step) const;
    Q_INVOKABLE QString portableKey(int key) const;
    Q_INVOKABLE int sequenceKey(const QString &text) const;
    Q_INVOKABLE QString sequenceText(int key) const;
Q_SIGNALS:
    void viewChanged();
private:
    void receive(const QJsonObject &snapshot);
    JsonInputPort &m_port;
    QVariantList m_macros;
    QString m_error, m_running;
    quint64 m_revision = 0;
    bool m_available = false, m_busy = false;
};
} // namespace QindaQt::Apps::SettingsInput

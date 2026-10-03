// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "controller_port.h"
#include <QJsonArray>
#include <QVariantList>

namespace QindaQt::Apps::SettingsInput {
class ControllersModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY viewChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY viewChanged)
    Q_PROPERTY(QVariantList controllers READ controllers NOTIFY viewChanged)
    Q_PROPERTY(QString selectedId READ selectedId WRITE select NOTIFY viewChanged)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY viewChanged)
    Q_PROPERTY(QVariantList buttons READ buttons NOTIFY viewChanged)
    Q_PROPERTY(QVariantList actions READ actions CONSTANT)
    Q_PROPERTY(QString statusText READ statusText NOTIFY viewChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY viewChanged)
public:
    explicit ControllersModel(ControllerPort &port, QObject *parent = nullptr);
    bool available() const { return m_available; }
    bool busy() const { return m_busy; }
    QVariantList controllers() const;
    QString selectedId() const { return m_selected; }
    QVariantMap selected() const;
    QVariantList buttons() const;
    QVariantList actions() const;
    QString statusText() const;
    QString errorText() const { return m_error; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void select(const QString &id);
    Q_INVOKABLE bool setOption(const QString &key, const QVariant &value);
    Q_INVOKABLE bool setBinding(const QString &button, const QString &action);
    Q_INVOKABLE bool setShortcut(const QString &button, int key);
    Q_INVOKABLE void reset();
    Q_INVOKABLE QString sequenceText(int key) const;
    Q_INVOKABLE int sequenceKey(const QString &text) const;
Q_SIGNALS:
    void viewChanged();
private:
    bool submit(const QJsonObject &patch);
    void receive(const QJsonObject &snapshot);
    ControllerPort &m_port;
    QJsonArray m_rows;
    QString m_selected = QStringLiteral("default:xbox"), m_error;
    quint64 m_revision = 0;
    bool m_available = false, m_busy = false, m_steam = false;
};
} // namespace QindaQt::Apps::SettingsInput

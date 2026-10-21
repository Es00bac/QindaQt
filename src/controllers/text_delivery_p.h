// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QPointer>
#include <QTimer>

namespace QindaQt::Controllers {
// Borrowed GUI-thread platform boundary. insert() commits one bounded chunk to
// the current target synchronously and never schedules input itself.
class TextOutput {
public:
    virtual ~TextOutput() = default;
    virtual QObject *target() const = 0;
    virtual bool allowed() const = 0;
    virtual int chunkCodepoints() const = 0;
    virtual bool insert(const QString &text) = 0;
};
class TextDelivery final : public QObject {
    Q_OBJECT
public:
    explicit TextDelivery(TextOutput &output, QObject *parent = nullptr);
    void start(const QString &token, const QString &text);
    // Release of the trigger finishes normally; suppression/page changes call
    // cancel even if the original trigger has already been released.
    void cancel(const QString &controllerPrefix = {});
    bool pending(const QString &token) const { return m_token == token; }
    bool succeeded(const QString &token) const { return m_finishedToken == token && m_success; }
Q_SIGNALS:
    void failed();
private:
    void step();
    void finish(bool success);
    TextOutput &m_output;
    QTimer m_timer;
    QPointer<QObject> m_target;
    QList<uint> m_text;
    QString m_token, m_finishedToken;
    bool m_success = false;
    qsizetype m_offset = 0;
};
} // namespace QindaQt::Controllers

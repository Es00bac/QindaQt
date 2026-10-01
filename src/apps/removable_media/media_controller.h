// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "media_backend.h"
#include "media_preferences.h"
#include <QSet>
#include <optional>

namespace QindaQt::Apps::RemovableMedia {
class MediaController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList volumes READ volumes NOTIFY changed)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY changed)
    Q_PROPERTY(QVariantMap formatTarget READ formatTarget NOTIFY changed)
    Q_PROPERTY(QStringList formatTypes READ formatTypes NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    // Borrows both collaborators. Set watchInsertions=false in UI-only tests
    // and diagnostic inventory views so no remembered choice runs there.
    MediaController(MediaBackend &backend, MediaPreferences &preferences,
                    bool watchInsertions = true, QObject *parent = nullptr);
    QVariantList volumes() const;
    QVariantMap selected() const;
    QVariantMap formatTarget() const;
    QStringList formatTypes() const;
    bool available() const;
    bool busy() const;
    QString status() const;
    Q_INVOKABLE void select(const QString &token);
    Q_INVOKABLE void show(const QString &token = {});
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void mount(const QString &token, bool readOnly = false, bool openAfter = false);
    Q_INVOKABLE void open(const QString &token);
    Q_INVOKABLE void unmount(const QString &token);
    Q_INVOKABLE void remove(const QString &token);
    // ask, mount, read-only, ignore. Unknown or unstable identities are refused.
    Q_INVOKABLE void remember(const QString &token, const QString &mode);
    Q_INVOKABLE void requestFormat(const QString &token);
    Q_INVOKABLE void cancelFormat();
    Q_INVOKABLE void confirmFormat(const QString &filesystem, const QString &label,
                                  const QString &typedDevice);
    Q_INVOKABLE void unlock(const QString &token, const QString &passphrase);
    void notificationAction(const QString &token, const QString &action);
Q_SIGNALS:
    void changed();
    void windowRequested(const QString &token);
    void openPathRequested(const QString &path);
    void notificationRequested(const QString &token, const QString &summary,
                               const QString &body, const QStringList &actions);
    void notificationWithdrawn(const QString &token);
private:
    const Volume *find(const QString &token) const;
    void inventoryChanged();
    void submit(Request request, bool openAfter = false, const QString &rememberMode = {});
    void completed(const QString &token, bool success, const QString &message,
                   const QString &mountPath);
    void prompt(const Volume &volume);
    MediaBackend &m_backend;
    MediaPreferences &m_preferences;
    QVector<Volume> m_volumes;
    QSet<QString> m_seen;
    QString m_selected, m_status, m_pendingToken, m_rememberMode, m_rememberKey;
    std::optional<Volume> m_formatTarget;
    bool m_watchInsertions, m_openAfter = false;
};
} // namespace QindaQt::Apps::RemovableMedia

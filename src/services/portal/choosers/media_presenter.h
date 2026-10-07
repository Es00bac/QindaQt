// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/removable_media_client/media_source.h>
#include <QObject>
#include <optional>
// Request-scoped GUI-thread interest over borrowed public values. Closing
// withdraws selection/navigation only; accepted owner work continues.
class ChooserMediaPresenter final : public QObject {
    Q_OBJECT
public:
    ChooserMediaPresenter(QindaQt::RemovableMedia::MediaSource &, bool saving, QObject *parent = nullptr);
    [[nodiscard]] QindaQt::RemovableMedia::Snapshot snapshot() const { return m_source.snapshot(); }
    [[nodiscard]] bool busy() const;
    [[nodiscard]] bool canAccept() const { return !m_closed && !m_revoked && !m_readOnlySave; }
    [[nodiscard]] QString notice() const { return m_notice; }
    [[nodiscard]] QString restriction() const;
    [[nodiscard]] QString recoveryLabel() const;
    void setLocation(const QString &, bool deliberate = true);
    void open(const QString &handle);
    void request(const QString &handle, QindaQt::RemovableMedia::Action, bool openAfter = false);
    void recover();
    void openOwner();
    void closeInterest();
Q_SIGNALS:
    void changed();
    void navigateRequested(const QString &);
    void locationInvalidated();
private:
    struct Interest {
        QindaQt::RemovableMedia::Lineage lineage;
        QindaQt::RemovableMedia::Attachment attachment;
        QString root;
    };
    void sourceChanged();
    void finished(const QindaQt::RemovableMedia::OperationResult &);
    void openRow(const QindaQt::RemovableMedia::VolumeRow &);
    QindaQt::RemovableMedia::MediaSource &m_source;
    std::optional<Interest> m_location, m_openAfter;
    QString m_path, m_openPath, m_requestId, m_notice, m_inventoryNotice;
    bool m_saving = false, m_closed = false, m_revoked = false, m_readOnlySave = false;
};

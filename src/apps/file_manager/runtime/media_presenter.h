// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/removable_media_client/media_source.h>
#include <QPointer>
#include "trash_discovery.h"
#include <QVariantList>
#include <QVector>

namespace QindaQt::Apps::FileManager {
class FolderNavigations;
class NavigationController;
// Window-local GUI-thread presentation, borrowing the public source and tab
// set. Neither owns storage authority. Accepted work continues after this
// presenter dies; only its deferred navigation interest is withdrawn.
class MediaPresenter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed FINAL)
    Q_PROPERTY(QString state READ state NOTIFY changed FINAL)
    Q_PROPERTY(QString notice READ notice NOTIFY changed FINAL)
    Q_PROPERTY(QString recoveryLabel READ recoveryLabel NOTIFY changed FINAL)
    Q_PROPERTY(bool busy READ busy NOTIFY changed FINAL)
public:
    MediaPresenter(QindaQt::RemovableMedia::MediaSource &, FolderNavigations &,
                   QObject *parent = nullptr);
    [[nodiscard]] QVariantList rows() const;
    [[nodiscard]] QString state() const;
    [[nodiscard]] QString notice() const { return m_notice; }
    [[nodiscard]] QString recoveryLabel() const;
    [[nodiscard]] bool busy() const;
    Q_INVOKABLE void open(const QString &handle);
    // Read-only existing-store discovery/navigation; never mounts or creates.
    Q_INVOKABLE void openTrash(const QString &handle, const QString &filesPath);
    Q_INVOKABLE void mountReadOnly(const QString &handle);
    Q_INVOKABLE void unmount(const QString &handle);
    Q_INVOKABLE void remove(const QString &handle);
    Q_INVOKABLE void details(const QString &handle);
    Q_INVOKABLE void recover();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void openOwner();
Q_SIGNALS:
    void changed();
private:
    struct LocationInterest {
        QPointer<NavigationController> navigation;
        QindaQt::RemovableMedia::Lineage lineage;
        QindaQt::RemovableMedia::Attachment attachment;
        QString root;
        bool revoked = false;
    };
    struct DeferredOpen {
        QPointer<NavigationController> navigation;
        QindaQt::RemovableMedia::Lineage lineage;
        QindaQt::RemovableMedia::Attachment attachment;
        QString path;
        quint64 generation = 0;
    };
    void request(const QString &, QindaQt::RemovableMedia::Action, bool openAfter);
    void openRow(const QindaQt::RemovableMedia::VolumeRow &, NavigationController &,
                 const QString &observedPath = {});
    void observeActive();
    void observeController(NavigationController *);
    void acquireLocation(NavigationController &, bool deliberate = false);
    void sourceChanged();
    void requestTrashDiscovery();
    void trashDiscovered(quint64, const QVariantMap &, const QString &);
    void navigationChanged();
    void finished(const QindaQt::RemovableMedia::OperationResult &);
    struct TrashOpenInterest {
        QPointer<NavigationController> navigation;
        QindaQt::RemovableMedia::Lineage lineage;
        QindaQt::RemovableMedia::Attachment attachment;
        QString root, filesPath, navigationPath;
        quint64 listingGeneration = 0;
    };
    TrashDiscovery m_trashDiscovery;
    QVariantMap m_trashPaths;
    std::optional<TrashOpenInterest> m_trashOpen;
    quint64 m_trashGeneration = 0;
    bool m_trashPending = false;
    QString m_trashDiagnostic;
    QindaQt::RemovableMedia::MediaSource &m_source;
    FolderNavigations &m_navigations;
    QVector<LocationInterest> m_locations;
    std::optional<DeferredOpen> m_openAfter;
    QString m_requestId, m_notice, m_inventoryNotice;
    bool m_opening = false, m_reconciling = false;
};
}

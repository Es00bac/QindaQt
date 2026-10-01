// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/chooser_ui.h>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QHash>
namespace QindaQt::Services::Portal {
// Public catalog provider resamples installed choices on requests/updates.
// Borrowed registry/UI/provider dependencies outlive this same-thread adaptor.
// Version1 implements ChooseApplication and UpdateChoices, without promising
// version2 activation-token generation or application launching.
class AppChooserAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.AppChooser")
    Q_PROPERTY(uint version READ version CONSTANT)
public:
    using Catalog = std::function<QindaQt::ApplicationCatalog::DirectoryScan()>;
    AppChooserAdaptor(QObject &, RequestRegistry &, ChooserUi &, Catalog);
    ~AppChooserAdaptor() override;
    uint version() const { return 1; }
public Q_SLOTS:
    quint32 ChooseApplication(const QDBusObjectPath &, const QString &, const QString &, const QStringList &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
    void UpdateChoices(const QDBusObjectPath &, const QStringList &, const QDBusMessage &);
private:
    RequestRegistry &m_requests; ChooserUi &m_ui; Catalog m_catalog;
    QHash<RequestToken, AppChooserRequest> m_pending;
    QHash<QString, RequestToken> m_handles;
};
} // namespace QindaQt::Services::Portal

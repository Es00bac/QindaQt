// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/chooser_ui.h>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QHash>
namespace QindaQt::Services::Portal {
// Borrowed same-thread registry/UI outlive adaptor; only own tokens retired.
// All three version4 methods publish through registered delayed Requests.
class FileChooserAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")
    Q_PROPERTY(uint version READ version CONSTANT)
public:
    FileChooserAdaptor(QObject &, RequestRegistry &, ChooserUi &);
    ~FileChooserAdaptor() override;
    uint version() const { return 4; }
public Q_SLOTS:
    quint32 OpenFile(const QDBusObjectPath &, const QString &, const QString &, const QString &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
    quint32 SaveFile(const QDBusObjectPath &, const QString &, const QString &, const QString &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
    quint32 SaveFiles(const QDBusObjectPath &, const QString &, const QString &, const QString &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
private:
    quint32 begin(FileChooserMode, const QDBusObjectPath &, const QString &, const QString &, const QString &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
    RequestRegistry &m_requests; ChooserUi &m_ui;
    QHash<RequestToken, FileChooserRequest> m_pending;
};
} // namespace QindaQt::Services::Portal

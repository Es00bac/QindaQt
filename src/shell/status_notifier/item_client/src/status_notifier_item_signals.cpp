// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/shell/status_notifier/item_client/status_notifier_item_client.h>
#include <qindaqt/shell/status_notifier/status_notifier_limits.h>

namespace QindaQt::StatusNotifier {
void StatusNotifierItemClient::handleNewTitle()
{
    scheduleRefetch();
}

void StatusNotifierItemClient::handleNewIcon()
{
    scheduleRefetch();
}

void StatusNotifierItemClient::handleNewAttentionIcon()
{
    scheduleRefetch();
}

void StatusNotifierItemClient::handleNewOverlayIcon()
{
    scheduleRefetch();
}

void StatusNotifierItemClient::handleNewToolTip()
{
    scheduleRefetch();
}

void StatusNotifierItemClient::handleNewStatus(const QString &status)
{
    Q_UNUSED(status)
    scheduleRefetch();
}

void StatusNotifierItemClient::handleNewIconThemePath(const QString &path)
{
    Q_UNUSED(path)
    scheduleRefetch();
}

void StatusNotifierItemClient::handleNewMenu()
{
    emit menuDetailsInvalidated();
    scheduleRefetch();
}

void StatusNotifierItemClient::handlePropertiesChanged(const QString &interface,
                                                       const QVariantMap &changed,
                                                       const QStringList &invalidated)
{
    if (interface != QString::fromLatin1(kItemInterfaceName)) {
        return;
    }
    if (changed.contains(QStringLiteral("Menu")) || changed.contains(QStringLiteral("ItemIsMenu"))
        || invalidated.contains(QStringLiteral("Menu")) || invalidated.contains(QStringLiteral("ItemIsMenu"))) {
        emit menuDetailsInvalidated();
    }
    scheduleRefetch();
}

void StatusNotifierItemClient::scheduleRefetch()
{
    if (m_fetchInFlight) {
        m_fetchDirty = true;
        return;
    }
    fetchDescriptor();
}

} // namespace QindaQt::StatusNotifier

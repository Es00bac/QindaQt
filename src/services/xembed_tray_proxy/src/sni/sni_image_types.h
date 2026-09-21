// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtDBus/QDBusArgument>
#include <QtCore/QMetaType>

namespace QindaQt::XEmbedTray
{

// AGENT-CONTRACT: the StatusNotifierItem wire type for IconPixmap is
// a(iiay) — width, height, premultiplied ARGB32 bytes. This struct plus its
// QDBusArgument operators are that wire shape; the element order and types
// must not change. Consumers: this module's item adaptor (producer) and the
// shell's item client (validator).
struct SniImage
{
    qint32 width = 0;
    qint32 height = 0;
    QByteArray data;
};

using SniImageList = QList<SniImage>;

QDBusArgument &operator<<(QDBusArgument &argument, const SniImage &image);
const QDBusArgument &operator>>(const QDBusArgument &argument, SniImage &image);

void registerSniImageTypes();

} // namespace QindaQt::XEmbedTray

Q_DECLARE_METATYPE(QindaQt::XEmbedTray::SniImage)
Q_DECLARE_METATYPE(QindaQt::XEmbedTray::SniImageList)

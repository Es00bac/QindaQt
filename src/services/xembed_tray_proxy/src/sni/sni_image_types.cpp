// SPDX-License-Identifier: GPL-3.0-or-later

#include "sni_image_types.h"

#include <QtDBus/QDBusMetaType>

namespace QindaQt::XEmbedTray
{

QDBusArgument &operator<<(QDBusArgument &argument, const SniImage &image)
{
    argument.beginStructure();
    argument << image.width << image.height << image.data;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SniImage &image)
{
    argument.beginStructure();
    argument >> image.width >> image.height >> image.data;
    argument.endStructure();
    return argument;
}

void registerSniImageTypes()
{
    qDBusRegisterMetaType<SniImage>();
    qDBusRegisterMetaType<SniImageList>();
}

} // namespace QindaQt::XEmbedTray

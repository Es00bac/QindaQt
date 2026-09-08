// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt_platform_theme.h"
#include <qpa/qplatformthemeplugin.h>

class QindaQtThemePlugin final : public QPlatformThemePlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformThemeFactoryInterface_iid FILE "qindaqt.json")
public:
    QPlatformTheme *create(const QString &key, const QStringList &) override {
        return key.compare(QStringLiteral("qindaqt"), Qt::CaseInsensitive) == 0
            ? new QindaQt::QtTheme::PlatformTheme : nullptr;
    }
};
#include "plugin.moc"

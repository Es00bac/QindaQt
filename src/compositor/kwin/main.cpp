// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqtkwinplugin.h"

#include <plugin.h>

class KWIN_EXPORT QindaQtPluginFactory final : public KWin::PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)

public:
    [[nodiscard]] std::unique_ptr<KWin::Plugin> create() const override
    {
        return std::make_unique<QindaQt::Compositor::KWinIntegration::QindaQtKWinPlugin>();
    }
};

#include "main.moc"

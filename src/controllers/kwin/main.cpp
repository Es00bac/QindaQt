// SPDX-License-Identifier: GPL-3.0-or-later
#include "controller_plugin.h"
#include <plugin.h>
class KWIN_EXPORT QindaQtControllerFactory final : public KWin::PluginFactory {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)
public:
    std::unique_ptr<KWin::Plugin> create() const override {
        return std::make_unique<QindaQt::Controllers::ControllerPlugin>();
    }
};
#include "main.moc"

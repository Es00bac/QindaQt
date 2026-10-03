// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "controller_input_device.h"
#include "qindaqt/controllers/controller_runtime.h"
#include <plugin.h>
#include <QPointer>
#include <QTimer>

namespace QindaQt::Controllers {
class ControllerEndpoint final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Controllers1")
public:
    explicit ControllerEndpoint(ControllerRuntime &runtime);
public Q_SLOTS:
    QString GetSnapshot() const;
    QString Apply(const QString &id, const QString &patch, qulonglong revision);
    QString Reset(const QString &id, qulonglong revision);
Q_SIGNALS:
    void Changed(qulonglong revision);
private:
    ControllerRuntime &m_runtime;
};
class ControllerPlugin final : public KWin::Plugin {
    Q_OBJECT
public:
    ControllerPlugin();
    ~ControllerPlugin() override;
private:
    void refreshContext();
    void reconcileTouchpads();
    ControllerInputDevice m_input;
    ControllerRuntime m_runtime;
    ControllerEndpoint m_endpoint;
    QTimer m_context;
    QMap<KWin::InputDevice *, bool> m_touchpads;
    bool m_published = false;
};
} // namespace QindaQt::Controllers

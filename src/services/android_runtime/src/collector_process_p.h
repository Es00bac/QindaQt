// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/services/android_runtime/descriptor_runtime_port.h"
#include <QProcess>
#include <QTimer>
#include <functional>
namespace QindaQt::Services::AndroidRuntime {
// Owning module/test-only already-admitted FD entry. Production supplies fixed
// protected executable and native session/recipe guard; never exposed to argv.
// Guard captures outlive this process; invoked on the owning Qt thread.
class CollectorProcess final:public RuntimePort {
public:
    explicit CollectorProcess(std::function<bool()> contextLive);
    ~CollectorProcess() override;
    bool start(int admittedExecutableFd);
    RuntimeSnapshot snapshot() const override;
    void requestRefresh() override;
    LaunchSubmission requestLaunch(const LaunchRequest &request) override;
    bool admitted() const;
private:
    std::function<bool()> m_live;
    QProcess m_process;
    QTimer m_guardTimer;
    std::unique_ptr<DescriptorRuntimePort> m_port;
    int m_executable=-1;
};
} // namespace QindaQt::Services::AndroidRuntime

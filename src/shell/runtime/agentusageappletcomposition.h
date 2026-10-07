// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <memory>
#include <functional>
namespace QindaQt::Applets { class ManifestCatalog; }
namespace QindaQt::AppletHost { class CapabilityPolicy; }
namespace QindaQt::Services::AgentUsage { class AgentUsageSource; }
namespace QindaQt::Shell::AgentUsageApplet { class AgentUsageAppletController; }
namespace QindaQt::Shell {
// Audited composition owns collection; presentation borrows only AgentUsageSource.
// No collection starts during construction. Denied policy creates no collector.
class AgentUsageAppletComposition final {
public:
    // Optional test/embedding factory transfers source ownership to this composition;
    // it runs on the GUI thread only after audited read admission.
    using SourceFactory = std::function<std::unique_ptr<Services::AgentUsage::AgentUsageSource>()>;
    AgentUsageAppletComposition(const Applets::ManifestCatalog &catalog,
                                const AppletHost::CapabilityPolicy &policy,
                                SourceFactory factory = {});
    ~AgentUsageAppletComposition();
    AgentUsageAppletComposition(const AgentUsageAppletComposition &) = delete;
    AgentUsageAppletComposition &operator=(const AgentUsageAppletComposition &) = delete;
    // Borrowed GUI-thread facade; valid until composition teardown after panels.
    AgentUsageApplet::AgentUsageAppletController *access() const noexcept;
private:
    std::unique_ptr<Services::AgentUsage::AgentUsageSource> m_source;
    std::unique_ptr<AgentUsageApplet::AgentUsageAppletController> m_access;
};
}

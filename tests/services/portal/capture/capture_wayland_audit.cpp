// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <cstring>
#include <memory>
#include <vector>
#include <unistd.h>
#include <wayland-client.h>

namespace {
struct RegistryObserver { wl_registry_listener original; void *data; };
std::vector<std::unique_ptr<RegistryObserver>> observers;
void record(const QByteArray &event) {
    QFile file(QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)).filePath("qindaqt-capture.audit"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Append))
        file.write(QByteArray::number(getpid()) + " " + event + '\n');
}
const wl_registry_listener observed{
    [](void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
        auto &observer = *static_cast<RegistryObserver *>(data);
        record("capture fd5 global " + QByteArray(interface));
        observer.original.global(observer.data, registry, name, interface, version);
    },
    [](void *data, wl_registry *registry, uint32_t name) {
        auto &observer = *static_cast<RegistryObserver *>(data);
        observer.original.global_remove(observer.data, registry, name);
    }};
}
extern "C" int __real_wl_proxy_add_listener(wl_proxy *, void (**)(void), void *);
extern "C" int __wrap_wl_proxy_add_listener(wl_proxy *proxy, void (**listener)(void), void *data) {
    // AGENT-GUARD: Link wrapping exists only in the non-installable helper.
    // Observe the static capture port's real fd5 registry and forward every
    // event unchanged; Qt's shared-library ordinary fd4 is never intercepted.
    if (strcmp(wl_proxy_get_class(proxy), "wl_registry") != 0
        || wl_display_get_fd(wl_proxy_get_display(proxy)) != 5)
        return __real_wl_proxy_add_listener(proxy, listener, data);
    auto observer = std::make_unique<RegistryObserver>();
    observer->original = *reinterpret_cast<wl_registry_listener *>(listener); observer->data = data;
    const auto result = __real_wl_proxy_add_listener(proxy,
        reinterpret_cast<void (**)(void)>(const_cast<wl_registry_listener *>(&observed)), observer.get());
    if (result == 0) observers.push_back(std::move(observer));
    return result;
}
namespace QindaQt::Services::Portal {
void captureTestControlEvent(const char *event) { record(QByteArray("control ")+event); }
}

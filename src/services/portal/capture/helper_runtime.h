// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QObject>
#include <memory>
namespace QindaQt::Services::Portal {
// Owns the fixed inherited helper channels, GUI/capture ports and imported
// parent. Same QApplication thread; production has no test input or launch
// fallback. Its protected control stays alive after a Screenshot result until
// broker retirement; any actual lineage loss stops bytes and capture first.
class HelperRuntime final : public QObject {
public:
    explicit HelperRuntime(QDBusConnection);
    ~HelperRuntime() override;
    bool start();
private:
    class Private;
    std::unique_ptr<Private> d;
};
}

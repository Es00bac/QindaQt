#pragma once
#include <memory>
#include <QString>
namespace QindaQt::SessionSupervisor {
class NativeLockComposition final {
public:
  NativeLockComposition();
  ~NativeLockComposition();
  bool start(QString *error = nullptr);
  void stop();
private:
  class Private;
  std::unique_ptr<Private> d;
};
}

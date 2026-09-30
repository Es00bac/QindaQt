// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QLabel>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <qindaqt/application_window_management/client.h>
#include <vector>
using namespace QindaQt::ApplicationWindowManagement;
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  WindowPlacementClient client;
  std::vector<std::unique_ptr<QWidget>> windows;
  auto make = [&](bool show) {
    auto value = std::make_unique<QWidget>();
    auto *window = value.get();
    window->setWindowTitle(
        QStringLiteral("Native placement probe %1").arg(windows.size() + 1));
    window->resize(640, 400);
    window->winId();
    windows.push_back(std::move(value));
    if (show)
      window->show();
    return window;
  };
  auto *source = make(true);
  QWidget *second = nullptr, *third = nullptr;
  int step = 0, burst = 0, limited = 0;
  quint32 pending = 0;
  QString container;
  QWidget *foreground = nullptr;
  const auto submit = [&](QWidget *from, QWidget *to, Placement mode,
                          bool cancel) {
    QString error;
    pending =
        client.place(from->windowHandle(), to->windowHandle(), mode, &error);
    if (!pending) {
      std::fprintf(stderr, "submission refused: %s\n", qPrintable(error));
      app.exit(2);
    } else if (cancel)
      client.cancel(pending);
  };
  std::function<void()> advance;
  advance = [&] {
    switch (step) {
    case 0:
      second = make(true);
      QTimer::singleShot(
          200, &app, [&] { submit(source, second, Placement::Tab, false); });
      break;
    case 1:
      third = make(true);
      QTimer::singleShot(200, &app, [&] {
        submit(second, third, Placement::TileRight, false);
      });
      break;
    case 2:
      submit(third, second, Placement::Tab, false);
      break;
    case 3:
      submit(third, make(false), Placement::Tab, true);
      break;
    case 4:
      submit(third, make(false), Placement::Tab, false);
      break;
    case 5:
      foreground = make(true);
      QTimer::singleShot(200, &app, [&] {
        submit(third, make(false), Placement::Tab, false);
      });
      break;
    case 6: {
      auto *created = make(false);
      submit(foreground, created, Placement::Tab, false);
      windows.pop_back();
      break;
    }
    case 7:
      submit(foreground, third, Placement::Tab, false);
      break;
    default:
      const auto actual=source->windowHandle()->devicePixelRatio();
      std::printf("ACTUAL_PLACEMENT_DPR=%.2f\n",actual);
      if(std::abs(actual-qEnvironmentVariable("APP_PLACEMENT_EXPECTED_SCALE").toDouble())>0.01) { app.exit(3);return; }
      std::printf("NATIVE_PLACEMENT_COMPLETED\n");
      std::fflush(stdout);
      QTimer::singleShot(1200, &app, [&] { app.exit(0); });
      break;
    }
  };
  QObject::connect(
      &client, &WindowPlacementClient::finished, &app,
      [&](quint32 id, Status status, const QString &owner,
          const QString &message) {
        if (id != pending)
          return;
        const Status expected[] = {Status::Accepted, Status::Accepted,
                                   Status::Invalid,  Status::Cancelled,
                                   Status::Timeout,  Status::Denied};
        std::printf("step=%d status=%u container=%s message=%s\n", step,
                    static_cast<unsigned>(status), qPrintable(owner),
                    qPrintable(message));
        std::fflush(stdout);
        if (step == 7) {
          if (status != Status::Invalid && status != Status::ResourceLimit) {
            app.exit(3);
            return;
          }
          if (status == Status::ResourceLimit)
            ++limited;
          if (++burst < 70) {
            QTimer::singleShot(0, &app, advance);
            return;
          }
          if (!limited) {
            app.exit(5);
            return;
          }
        } else if ((step == 6 && status != Status::Denied &&
                    status != Status::Cancelled) ||
                   (step < 6 && status != expected[step]) ||
                   (step == 1 && owner != container)) {
          app.exit(3);
          return;
        }
        if (step == 0)
          container = owner;
        ++step;
        QTimer::singleShot(100, &app, advance);
      });
  QTimer::singleShot(1000, &app, advance);
  QTimer::singleShot(20000, &app, [&] { app.exit(4); });
  return app.exec();
}

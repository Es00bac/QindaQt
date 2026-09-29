// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QLabel>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <cstdio>
#include <qindaqt/application_window_management/client.h>
using namespace QindaQt::ApplicationWindowManagement;
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  const bool exercise = app.arguments().contains(QStringLiteral("--exercise"));
  WindowPlacementClient placement;
  int serial = 0, accepted = 0;
  QWidget *latest = nullptr;
  std::function<QWidget *()> makeWindow;
  makeWindow = [&]() {
    auto *window = new QWidget;
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setWindowTitle(
        QStringLiteral("Native placement example %1").arg(++serial));
    window->resize(640, 400);
    auto *layout = new QVBoxLayout(window);
    layout->addWidget(new QLabel(
        QStringLiteral("Ctrl+T: new container tab\nCtrl+Shift+T: new tile to "
                       "the right\n\nThe application creates each window. "
                       "QindaQt owns placement."),
        window));
    const auto create = [&, window](Placement mode) {
      auto *created = makeWindow();
      QTimer::singleShot(0, created, [&, window, created, mode] {
        QString error;
        if (!placement.place(window->windowHandle(), created->windowHandle(),
                             mode, &error)) {
          std::fprintf(stderr, "placement unavailable: %s\n",
                       qPrintable(error));
          if (exercise)
            app.exit(2);
        }
      });
    };
    auto *tab = new QShortcut(QKeySequence(QStringLiteral("Ctrl+T")), window);
    QObject::connect(tab, &QShortcut::activated, window,
                     [create] { create(Placement::Tab); });
    auto *tile =
        new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+T")), window);
    QObject::connect(tile, &QShortcut::activated, window,
                     [create] { create(Placement::TileRight); });
    window->show();
    latest = window;
    return window;
  };
  auto *initial = makeWindow();
  QObject::connect(
      &placement, &WindowPlacementClient::finished, &app,
      [&](quint32 id, Status status, const QString &container,
          const QString &message) {
        std::printf("request=%u status=%u container=%s message=%s\n", id,
                    static_cast<unsigned>(status), qPrintable(container),
                    qPrintable(message));
        std::fflush(stdout);
        if (!exercise)
          return;
        if (status != Status::Accepted) {
          app.exit(3);
          return;
        }
        if (++accepted == 2) {
          app.exit(0);
          return;
        }
        auto *source = latest, *created = makeWindow();
        QTimer::singleShot(100, created, [&, source, created] {
          QString error;
          if (!placement.place(source->windowHandle(), created->windowHandle(),
                               Placement::TileRight, &error))
            app.exit(4);
        });
      });
  if (exercise) {
    QTimer::singleShot(1000, initial, [&] {
      auto *created = makeWindow();
      QString error;
      if (!placement.place(initial->windowHandle(), created->windowHandle(),
                           Placement::Tab, &error)) {
        std::fprintf(stderr, "%s\n", qPrintable(error));
        app.exit(5);
      }
    });
    QTimer::singleShot(15000, &app, [&] { app.exit(6); });
  }
  return app.exec();
}

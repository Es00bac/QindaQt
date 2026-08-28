// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_appearance.h"
#include "ui/terminal_widget_adapter.h"

#include "qindaqt/themes/theme_loader.h"

#include <QCoreApplication>
#include <QImage>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

using QindaQt::Apps::Terminal::TerminalAppearanceAdapter;
using QindaQt::Apps::Terminal::TerminalSessionBackend;
using QindaQt::Apps::Terminal::TerminalViewAppearance;
using QindaQt::Apps::Terminal::TerminalWidgetAdapter;

namespace {

TerminalViewAppearance darkAppearance() {
  const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  if (!theme.ok) {
    qFatal("Could not load qinda-dark: %s", qPrintable(theme.error));
  }
  const auto appearance = TerminalAppearanceAdapter::fromTheme(theme.theme);
  if (!appearance.ok()) {
    qFatal("Could not derive Terminal appearance: %s",
           qPrintable(appearance.diagnostic));
  }
  return *appearance.appearance;
}

} // namespace

class TerminalWidgetAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void blankGridDoesNotPublishCopyAvailability();
  void customSchemePaintsRequestedTerminalBackground();
};

void TerminalWidgetAdapterTest::blankGridDoesNotPublishCopyAvailability() {
  TerminalWidgetAdapter adapter(darkAppearance());
  QSignalSpy selectionSpy(&adapter,
                          &TerminalSessionBackend::selectionChanged);

  adapter.selectAllInView();

  QCOMPARE(selectionSpy.count(), 1);
  QCOMPARE(selectionSpy.at(0).at(0).toBool(), false);
  QVERIFY(!adapter.hasSelectedText());
}

void TerminalWidgetAdapterTest::customSchemePaintsRequestedTerminalBackground() {
  const TerminalViewAppearance appearance = darkAppearance();
  QCOMPARE(appearance.terminalBackground, QColor(QStringLiteral("#171a18")));

  // The adapter owns and deletes its widget. Declare the host first so the
  // adapter is destroyed first and QLayout never becomes a competing owner.
  QWidget host;
  TerminalWidgetAdapter adapter(appearance);
  auto *layout = new QVBoxLayout(&host);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(adapter.terminalWidget());
  host.resize(640, 400);
  host.show();
  QVERIFY(QTest::qWaitForWindowExposed(&host));
  QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

  const QImage rendered = host.grab().toImage();
  QVERIFY(!rendered.isNull());
  const QColor center = QColor::fromRgba(
      rendered.pixel(rendered.width() / 2, rendered.height() / 2));
  QCOMPARE(center.rgb(), appearance.terminalBackground.rgb());
}

QTEST_MAIN(TerminalWidgetAdapterTest)
#include "tst_terminal_widget_adapter.moc"

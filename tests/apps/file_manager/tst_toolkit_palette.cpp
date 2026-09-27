// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QPalette>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QtTest>
#include <memory>

class ToolkitPaletteTests final : public QObject {
  Q_OBJECT
private slots:
  void inheritedApplicationPaletteReachesFileViews();
};

void ToolkitPaletteTests::inheritedApplicationPaletteReachesFileViews()
{
  const QPalette original = QGuiApplication::palette();
  const auto restore = qScopeGuard([&] { QGuiApplication::setPalette(original); });
  QQmlEngine engine;
  QQmlComponent windowComponent(&engine);
  windowComponent.setData(R"(
import QtQuick
import QtQuick.Controls
import QindaTK as Tk
ApplicationWindow {
    width: 400; height: 260; visible: true
    property color toolkitText: Tk.Theme.color.text
    property color toolkitMuted: Tk.Theme.color.textMuted
    property color toolkitBackground: Tk.Theme.color.bg
    property color toolkitPanel: Tk.Theme.color.panel
    property color toolkitSelection: Tk.Theme.color.accent
    property color toolkitSelectionText: Tk.Theme.color.accentContrast
    property color inheritedText: palette.text
    Tk.Thumbnail { x: 20; y: 20; width: 80; height: 80 }
    Tk.Label { x: 20; y: 120; text: "Document" }
})", QUrl(QStringLiteral("inmemory:/FilePaletteWindow.qml")));
  QTRY_VERIFY(windowComponent.status() != QQmlComponent::Loading);
  QVERIFY2(windowComponent.isReady(), qPrintable(windowComponent.errorString()));
  std::unique_ptr<QObject> window(windowComponent.create());
  QVERIFY2(window, qPrintable(windowComponent.errorString()));
  QQmlComponent bridgeComponent(&engine, QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/src/apps/file_manager/ui/ToolkitTheme.qml")));
  QTRY_VERIFY(bridgeComponent.status() != QQmlComponent::Loading);
  QVERIFY2(bridgeComponent.isReady(), qPrintable(bridgeComponent.errorString()));
  std::unique_ptr<QObject> bridge(bridgeComponent.createWithInitialProperties(
      {{QStringLiteral("window"), QVariant::fromValue(window.get())}}));
  QVERIFY2(bridge, qPrintable(bridgeComponent.errorString()));

  // Change the application palette, not an explicit window role: only this
  // reproduces the live platform-theme transition that stranded the role map.
  for (bool dark : {true, false, true, false}) {
    QPalette palette = original;
    const QColor background(dark ? "#25232b" : "#c3c3bb");
    const QColor text(dark ? "#f7f3ed" : "#201e24");
    const QColor muted(dark ? "#c7c1d0" : "#48434f");
    const QColor panel(dark ? "#302d37" : "#e0dfd5");
    const QColor accent(dark ? "#d8b7ec" : "#664077");
    const QColor accentText(dark ? "#25192b" : "#ffffff");
    palette.setColor(QPalette::Window, background);
    palette.setColor(QPalette::Base, panel);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::PlaceholderText, muted);
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, accentText);
    QGuiApplication::setPalette(palette);
    QTRY_COMPARE(window->property("inheritedText").value<QColor>(), text);
    QTRY_COMPARE(window->property("toolkitText").value<QColor>(), text);
    QCOMPARE(window->property("toolkitMuted").value<QColor>(), muted);
    QCOMPARE(window->property("toolkitBackground").value<QColor>(), background);
    QCOMPARE(window->property("toolkitPanel").value<QColor>(), panel);
    QCOMPARE(window->property("toolkitSelection").value<QColor>(), accent);
    QCOMPARE(window->property("toolkitSelectionText").value<QColor>(), accentText);
  }
}
QTEST_MAIN(ToolkitPaletteTests)
#include "tst_toolkit_palette.moc"

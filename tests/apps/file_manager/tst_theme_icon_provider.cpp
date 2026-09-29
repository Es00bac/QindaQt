// SPDX-License-Identifier: GPL-3.0-or-later
#include "preview/theme_icon_provider.h"
#include <QIcon>
#include <QPalette>
#include <QtTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QThread>

#include <memory>
using namespace QindaQt::Apps::FileManager;
namespace {
constexpr int kEdge = 64;
// A glyph pixel counts only when it is essentially opaque: antialiased edges
// carry the tint color at low alpha and say nothing about the tint itself.
[[nodiscard]] bool paintsColor(const QPixmap &pixmap, const QColor &target,
                               int tolerance) {
  const QImage image = pixmap.toImage();
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const QColor c = image.pixelColor(x, y);
      if (c.alpha() > 200 && qAbs(c.red() - target.red()) <= tolerance &&
          qAbs(c.green() - target.green()) <= tolerance &&
          qAbs(c.blue() - target.blue()) <= tolerance) {
        return true;
      }
    }
  }
  return false;
}
} // namespace
class IconChoiceState final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString iconTheme MEMBER iconTheme NOTIFY changed)
public:
  QString iconTheme;
signals:
  void changed();
};
class ThemeIconProviderTest : public QObject {
  Q_OBJECT
private slots:
  void liveChoiceRefreshesTheExistingImage();
  void symbolicFollowsThePalette();
  void symbolicHonorsAnExplicitQueryColor();
  void symbolicRejectsAMalformedQueryColor();
  void fullColorIconsKeepTheirAuthoredPixels();
  void unresolvedNamesRenderTheColoredFallback();
  void unresolvedSymbolicNamesDoNotTintTheFallback();
  void readerThreadRequestsRenderOnTheGuiThread();
};
void ThemeIconProviderTest::liveChoiceRefreshesTheExistingImage() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QStringList oldPaths = QIcon::themeSearchPaths();
  const QString oldTheme = QIcon::themeName();
  const auto restore = qScopeGuard([&] {
    QIcon::setThemeSearchPaths(oldPaths);
    QIcon::setThemeName(oldTheme);
  });
  for (const auto &name : {QStringLiteral("First"), QStringLiteral("Second")}) {
    const QString root = temporary.path() + "/" + name;
    QVERIFY(QDir().mkpath(root + "/32"));
    QFile index(root + "/index.theme");
    QVERIFY(index.open(QIODevice::WriteOnly));
    index.write("[Icon Theme]\nName=Fixture\nDirectories=32\n[32]\nSize=32\nType=Fixed\n");
    index.close();
    QImage pixels(32, 32, QImage::Format_ARGB32);
    pixels.fill(name == "First" ? Qt::red : Qt::blue);
    QVERIFY(pixels.save(root + "/32/folder.png"));
  }
  QIcon::setThemeSearchPaths({temporary.path()});
  QIcon::setThemeName("First");
  IconChoiceState appearance;
  appearance.iconTheme = "First";
  QQmlEngine engine;
  engine.rootContext()->setContextProperty("fileManagerIconAppearance", &appearance);
  auto *provider = new ThemeIconProvider;
  engine.addImageProvider("theme-icons", provider);
  QQmlComponent component(&engine, QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/src/apps/file_manager/ui/PlaceButton.qml")));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));
  std::unique_ptr<QObject> button(component.createWithInitialProperties({{"iconName", "folder"}}));
  QVERIFY2(button, qPrintable(component.errorString()));
  QObject *image = nullptr;
  for (auto *child : button->findChildren<QObject *>()) {
    if (child->property("source").toUrl().toString().startsWith("image://theme-icons/")) {
      image = child;
      break;
    }
  }
  QVERIFY(image);
  const QUrl before = image->property("source").toUrl();
  QTRY_COMPARE(image->property("status").toInt(), 1);
  QVERIFY(paintsColor(provider->requestPixmap("folder", nullptr, {32, 32}), Qt::red, 0));
  QIcon::setThemeName("Second");
  appearance.iconTheme = "Second";
  emit appearance.changed();
  QTRY_VERIFY(image->property("source").toUrl() != before);
  QTRY_COMPARE(image->property("status").toInt(), 1);
  QVERIFY(button->findChildren<QObject *>().contains(image));
  QVERIFY(paintsColor(provider->requestPixmap("folder", nullptr, {32, 32}), Qt::blue, 0));
}
void ThemeIconProviderTest::symbolicFollowsThePalette() {
  // The dark-theme contract: the authored #211D27 stroke would vanish on a
  // dark window, so the provider retints symbolic glyphs to the palette
  // foreground. Light themes keep working because WindowText is dark there.
  QPalette dark;
  dark.setColor(QPalette::Window, QColor(QStringLiteral("#26232B")));
  dark.setColor(QPalette::WindowText, QColor(QStringLiteral("#F4F1EA")));
  QGuiApplication::setPalette(dark);
  ThemeIconProvider provider;
  const QPixmap pixmap = provider.requestPixmap(
      QStringLiteral("go-previous-symbolic"), nullptr, QSize(kEdge, kEdge));
  QVERIFY(!pixmap.isNull());
  QVERIFY(paintsColor(pixmap, QColor(QStringLiteral("#F4F1EA")), 24));
  QVERIFY(!paintsColor(pixmap, QColor(QStringLiteral("#211D27")), 16));
}
void ThemeIconProviderTest::symbolicHonorsAnExplicitQueryColor() {
  // QML callers pass their palette color in the URL so a live theme switch
  // re-resolves the request; the provider trusts that color over the app
  // palette. "%23" is the URL-encoded '#'.
  ThemeIconProvider provider;
  const QPixmap pixmap = provider.requestPixmap(
      QStringLiteral("go-previous-symbolic?color=%23ff0000"), nullptr,
      QSize(kEdge, kEdge));
  QVERIFY(!pixmap.isNull());
  QVERIFY(paintsColor(pixmap, QColor(QStringLiteral("#ff0000")), 24));
}
void ThemeIconProviderTest::symbolicRejectsAMalformedQueryColor() {
  QPalette dark;
  dark.setColor(QPalette::WindowText, QColor(QStringLiteral("#F4F1EA")));
  QGuiApplication::setPalette(dark);
  ThemeIconProvider provider;
  const QPixmap pixmap = provider.requestPixmap(
      QStringLiteral("go-previous-symbolic?color=red"), nullptr,
      QSize(kEdge, kEdge));
  QVERIFY(!pixmap.isNull());
  QVERIFY(paintsColor(pixmap, QColor(QStringLiteral("#F4F1EA")), 24));
  QVERIFY(!paintsColor(pixmap, QColor(QStringLiteral("#ff0000")), 24));
}
void ThemeIconProviderTest::fullColorIconsKeepTheirAuthoredPixels() {
  // Only resolved "-symbolic" names recolor; pictograms keep their artwork.
  // A "?color=" query on a full-color name is ignored.
  ThemeIconProvider provider;
  const QPixmap pixmap = provider.requestPixmap(
      QStringLiteral("folder?color=%23ff0000"), nullptr, QSize(kEdge, kEdge));
  QVERIFY(!pixmap.isNull());
  // AGENT-NOTE: #E9A445 is the QindaQt folder body authored by
  // tools/qinda_icon_catalog_places.py (ADR-0283); update it with the artwork.
  QVERIFY(paintsColor(pixmap, QColor(QStringLiteral("#E9A445")), 24));
  QVERIFY(!paintsColor(pixmap, QColor(QStringLiteral("#ff0000")), 16));
}
void ThemeIconProviderTest::unresolvedNamesRenderTheColoredFallback() {
  // The pre-existing fallback contract: an unknown name paints the theme's
  // full-color application-octet-stream document rather than a null pixmap
  // (a null result makes QML Image warn, fatal under the offscreen
  // QT_FATAL_WARNINGS rows).
  ThemeIconProvider provider;
  const QPixmap pixmap = provider.requestPixmap(
      QStringLiteral("no-such-icon-name"), nullptr, QSize(kEdge, kEdge));
  QVERIFY(!pixmap.isNull());
  QVERIFY(paintsColor(pixmap, QColor(QStringLiteral("#9C86AA")), 24));
}
void ThemeIconProviderTest::unresolvedSymbolicNamesDoNotTintTheFallback() {
  // A "-symbolic" name missing from the theme falls back to the colored
  // document; recoloring that would destroy full-color artwork, so the tint
  // must stay off and the query color must not appear.
  QPalette dark;
  dark.setColor(QPalette::WindowText, QColor(QStringLiteral("#00ff00")));
  QGuiApplication::setPalette(dark);
  ThemeIconProvider provider;
  const QPixmap pixmap = provider.requestPixmap(
      QStringLiteral("no-such-icon-symbolic?color=%23ff0000"), nullptr,
      QSize(kEdge, kEdge));
  QVERIFY(!pixmap.isNull());
  QVERIFY(paintsColor(pixmap, QColor(QStringLiteral("#9C86AA")), 24));
  QVERIFY(!paintsColor(pixmap, QColor(QStringLiteral("#00ff00")), 16));
  QVERIFY(!paintsColor(pixmap, QColor(QStringLiteral("#ff0000")), 16));
}
// Live crash, 2026-09-28: Qt Quick's image-reader thread used QIcon while the
// GUI thread was rendering another icon, and QIcon's caches are not thread
// safe. A request made from another thread renders on the GUI thread and
// still returns the real artwork.
void ThemeIconProviderTest::readerThreadRequestsRenderOnTheGuiThread() {
  ThemeIconProvider provider;
  QPixmap pixmap;
  std::unique_ptr<QThread> reader(QThread::create([&provider, &pixmap] {
    pixmap = provider.requestPixmap(QStringLiteral("folder"), nullptr,
                                    QSize(kEdge, kEdge));
  }));
  reader->start();
  QTRY_VERIFY_WITH_TIMEOUT(reader->isFinished(), 5000);
  QVERIFY(reader->wait(1000));
  QVERIFY(!pixmap.isNull());
  QVERIFY(paintsColor(pixmap, QColor(QStringLiteral("#E9A445")), 24));
}
int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  QIcon::setThemeSearchPaths(
      {QStringLiteral(QINDAQT_SOURCE_DIR) + QStringLiteral("/data/icons")});
  QIcon::setThemeName(QStringLiteral("QindaQt"));
  ThemeIconProviderTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "tst_theme_icon_provider.moc"

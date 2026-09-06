// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Shared rig for the Display arrangement rows: the real DisplaySettingsModel
// over the fake transport (so topology admission is the production rule) and
// the real DisplayPage loaded from source with the QindaQt token facade.

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/themes/theme_loader.h"
#include "tests/services/display_client/support/fake_display_transport.h"

#include <qindaqt/apps/settings_display/display_settings_model.h>
#include <qindaqt/services/display_client/client.h>
#include <qindaqt/services/display_client/display_coordinator.h>
#include <qindaqt/services/display_protocol/display_types.h>

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QPoint>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QTest>
#include <QUrl>
#include <memory>

namespace QindaQt::Tests::DisplayArrangementSupport {

inline QindaQt::Display::Output makeOutput(const QString &stableId,
                                           const QString &connector,
                                           const QString &label,
                                           const QSize &pixels, double scale,
                                           const QPoint &position, bool primary,
                                           quint32 priority) {
  const QString modeId = QStringLiteral("%1x%2@60").arg(pixels.width()).arg(pixels.height());
  QindaQt::Display::Output output{
      .stableId = stableId,
      .connectorName = connector,
      .runtimeCompositorUuid = QStringLiteral("uuid-") + connector,
      .label = label,
      .manufacturer = QStringLiteral("Test"),
      .model = label,
      .physicalSizeMillimeters = QSize(600, 340),
      .enabled = true,
      .primary = primary,
      .modeId = modeId,
      .position = position,
      .logicalSize = QSize(int(pixels.width() / scale), int(pixels.height() / scale)),
      .scale = scale,
      .transform = QindaQt::Display::Transform::Normal,
      .priority = priority,
      .replicationSourceStableId = {},
      .modes = {
          {.id = modeId, .pixelSize = pixels, .refreshMilliHertz = 60000, .preferred = true},
      },
  };
  if (pixels != QSize(1920, 1080)) {
    output.modes.append({.id = QStringLiteral("1920x1080@60"),
                         .pixelSize = QSize(1920, 1080),
                         .refreshMilliHertz = 60000,
                         .preferred = false});
  }
  return output;
}

// 4K at 200% (logical 1920x1080) at the origin, a 1080p at 100% attached to
// its right edge. The mixed-density case the outcome must handle.
inline QindaQt::Display::Snapshot mixedDensitySnapshot() {
  return {
      .protocolVersion = 1,
      .serviceEpoch = QStringLiteral("ep1"),
      .revision = 1,
      .liveFingerprint = QByteArray(32, 1),
      .outputs = {
          makeOutput(QStringLiteral("edid:dp1"), QStringLiteral("DP-1"),
                     QStringLiteral("Main Monitor"), QSize(3840, 2160), 2.0,
                     QPoint(0, 0), true, 1),
          makeOutput(QStringLiteral("edid:hdmi1"), QStringLiteral("HDMI-1"),
                     QStringLiteral("Side Monitor"), QSize(1920, 1080), 1.0,
                     QPoint(1920, 0), false, 2),
      },
      .transactions = {},
  };
}

inline QindaQt::Display::Snapshot threeInARowSnapshot() {
  auto snapshot = mixedDensitySnapshot();
  snapshot.outputs.append(makeOutput(QStringLiteral("edid:dp2"), QStringLiteral("DP-2"),
                                     QStringLiteral("Far Monitor"), QSize(1920, 1080),
                                     1.0, QPoint(3840, 0), false, 3));
  return snapshot;
}

inline QindaQt::Display::Snapshot connectedDisabledSideSnapshot() {
  auto snapshot = mixedDensitySnapshot();
  auto &side = snapshot.outputs[1];
  side.enabled = false;
  side.primary = false;
  side.priority = 0;
  side.position = QPoint();
  return snapshot;
}

struct ModelRig {
  QindaQt::DisplayClient::TestSupport::FakeDisplayTransport transport;
  QindaQt::DisplayClient::Client client{&transport};
  QindaQt::DisplayClient::Coordinator coordinator{&client};
  QindaQt::Apps::SettingsDisplay::DisplaySettingsModel model{client, coordinator};

  bool publish(const QindaQt::Display::Snapshot &snapshot) {
    client.start();
    transport.publishOwner(QStringLiteral(":1.50"));
    if (transport.fetches.isEmpty()) {
      return false;
    }
    transport.replySnapshot(transport.fetches.first(), snapshot);
    return model.ready();
  }

  QVariantMap output(const QString &stableId) const {
    const auto outputs = model.outputs();
    for (const auto &entry : outputs) {
      const auto map = entry.toMap();
      if (map.value(QStringLiteral("stableId")).toString() == stableId) {
        return map;
      }
    }
    return {};
  }

  QPoint positionOf(const QString &stableId) const {
    const auto map = output(stableId);
    return {map.value(QStringLiteral("positionX")).toInt(),
            map.value(QStringLiteral("positionY")).toInt()};
  }
};

inline QQuickItem *findItemByObjectName(QQuickItem *root, const QString &name) {
  if (root == nullptr) {
    return nullptr;
  }
  if (root->objectName() == name) {
    return root;
  }
  for (QQuickItem *child : root->childItems()) {
    if (auto *match = findItemByObjectName(child, name); match != nullptr) {
      return match;
    }
  }
  return nullptr;
}

inline QList<QQuickItem *> itemsWithProperty(QQuickItem *root, const char *property) {
  QList<QQuickItem *> items;
  if (root == nullptr) {
    return items;
  }
  if (root->property(property).isValid()) {
    items.append(root);
  }
  for (QQuickItem *child : root->childItems()) {
    items.append(itemsWithProperty(child, property));
  }
  return items;
}

struct PageRig {
  std::unique_ptr<QObject> pageGuard;
  QQuickItem *page = nullptr;
};

inline bool bootstrapTokens(QQuickView &view, QString *error) {
  view.engine()->addImportPath(QString::fromUtf8(QINDAQT_QML_IMPORT_PATH));
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(*view.engine(), error);
  if (facade == nullptr) {
    return false;
  }
  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  if (!loaded.ok) {
    *error = loaded.error;
    return false;
  }
  return facade->publish(loaded.theme, {}, error);
}

inline PageRig loadPage(QQuickView &view, QObject *model, QString *error) {
  PageRig rig;
  QQmlComponent component(view.engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(QINDAQT_DISPLAY_PAGE_QML_PATH)));
  if (!component.isReady()) {
    *error = component.errorString();
    return rig;
  }
  QObject *pageObject = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"), QVariant::fromValue(model)},
  });
  if (pageObject == nullptr) {
    *error = component.errorString();
    return rig;
  }
  rig.pageGuard.reset(pageObject);
  rig.page = qobject_cast<QQuickItem *>(pageObject);
  if (rig.page == nullptr) {
    *error = QStringLiteral("page is not an item");
    return rig;
  }
  view.resize(960, 1240);
  rig.page->setParentItem(view.contentItem());
  rig.page->setSize(view.size());
  view.show();
  QCoreApplication::processEvents();
  return rig;
}

inline QPoint sceneCenter(QQuickItem *item) {
  return item->mapToScene(QPointF(item->width() / 2.0, item->height() / 2.0)).toPoint();
}

inline void dragBy(QQuickView &view, QQuickItem *tile, const QPoint &delta) {
  const QPoint start = sceneCenter(tile);
  QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, start);
  constexpr int steps = 6;
  for (int step = 1; step <= steps; ++step) {
    QTest::mouseMove(&view, start + delta * step / steps, 20);
  }
  QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, start + delta);
  QCoreApplication::processEvents();
}

// Optional render evidence: set QINDAQT_DISPLAY_ARRANGEMENT_RENDER_DIR to a
// writable directory to keep a PNG of the offscreen page per checkpoint.
inline void keepRender(QQuickView &view, const QString &name) {
  const QString directory =
      qEnvironmentVariable("QINDAQT_DISPLAY_ARRANGEMENT_RENDER_DIR");
  if (directory.isEmpty()) {
    return;
  }
  QDir().mkpath(directory);
  const QImage image = view.grabWindow();
  image.save(QDir(directory).filePath(name + QStringLiteral(".png")));
}

} // namespace QindaQt::Tests::DisplayArrangementSupport

// SPDX-License-Identifier: GPL-3.0-or-later

// The Power supplies section's charge meter (plan W2): a Tk.Meter bound to
// the projection's numeric `percentage`, drawn only while `percentageKnown`
// is true, in the neutral accent unless Power itself raises a warning. The
// section is loaded on its own so these rows do not grow the page suite.

#include "stub_power_settings_model.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtGui/QColor>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include <memory>

using QindaQt::Apps::SettingsPower::TestSupport::StubPowerSettingsModel;

namespace {

QQuickItem *findItem(QQuickItem *root, const QString &name) {
  if (root == nullptr) return nullptr;
  if (root->objectName() == name) return root;
  for (QQuickItem *child : root->childItems())
    if (QQuickItem *found = findItem(child, name)) return found;
  return nullptr;
}

QVariantMap batteryRow(const QString &id, bool known, double percentage,
                       quint32 severity,
                       const QString &name = QStringLiteral("Main battery"),
                       const QString &state = QStringLiteral("Discharging")) {
  return {{QStringLiteral("id"), id},
          {QStringLiteral("name"), name},
          {QStringLiteral("kindText"), QStringLiteral("Battery")},
          {QStringLiteral("stateText"), state},
          {QStringLiteral("percentageText"), known
               ? QStringLiteral("%1 percent").arg(percentage, 0, 'f', 0)
               : QStringLiteral("Level Low")},
          {QStringLiteral("percentageKnown"), known},
          {QStringLiteral("percentage"), known ? percentage : 0.0},
          {QStringLiteral("timeText"), QString{}},
          {QStringLiteral("warningText"), QStringLiteral("Warning")},
          {QStringLiteral("warningSeverity"), severity},
          {QStringLiteral("accessibleDescription"), QStringLiteral("Battery")}};
}

} // namespace

class PowerSupplyMeterTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void knownChargeDrawsABoundAccentMeter();
  void unknownChargeAndTheAdapterDrawNoMeter();
  void onlyAPowerWarningChangesTheMeterColour();
  void metersShareOneColumnAcrossRows();

private:
  std::unique_ptr<QQuickView> m_view;
  std::unique_ptr<StubPowerSettingsModel> m_model;
  std::unique_ptr<QObject> m_section;
  std::unique_ptr<QObject> m_themeProbe;

  QQuickItem *createSection(const QVariantList &rows);
  [[nodiscard]] QColor themeColour(const char *role) const;
};

void PowerSupplyMeterTest::initTestCase() {
  m_view = std::make_unique<QQuickView>();
  m_view->engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
  QString error;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
      *m_view->engine(), &error);
  QVERIFY2(facade != nullptr, qPrintable(error));
  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  QVERIFY2(loaded.ok, qPrintable(loaded.error));
  QVERIFY2(facade->publish(loaded.theme, {}, &error), qPrintable(error));
}

QQuickItem *PowerSupplyMeterTest::createSection(const QVariantList &rows) {
  m_section.reset();
  m_model = std::make_unique<StubPowerSettingsModel>();
  m_model->supplyRows = rows;
  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_POWER_SUPPLY_SECTION_QML_PATH)));
  if (!component.isReady()) {
    qWarning().noquote() << component.errorString();
    return nullptr;
  }
  m_section.reset(component.createWithInitialProperties({
      {QStringLiteral("powerSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  }));
  auto *section = qobject_cast<QQuickItem *>(m_section.get());
  if (section == nullptr) return nullptr;
  m_view->resize(QSize(720, 480));
  section->setParentItem(m_view->contentItem());
  section->setWidth(720);
  m_view->show();
  QCoreApplication::processEvents();

  // The section's own QindaQtTheme bridge has fed Tk.Theme by now; read the
  // accent back through that singleton and the warning hues from the desktop
  // tokens, exactly as the meter binds them.
  QQmlComponent probe(m_view->engine());
  probe.setData("import QtQuick\nimport QindaQt.Tokens 1.0\n"
                "import QindaTK as Tk\nQtObject {\n"
                "  property color accent: Tk.Theme.color.accent\n"
                "  property color warning: Tokens.status.warning.background\n"
                "  property color danger: Tokens.danger.default\n}\n",
                QUrl());
  m_themeProbe.reset(probe.create());
  return section;
}

QColor PowerSupplyMeterTest::themeColour(const char *role) const {
  return m_themeProbe != nullptr ? m_themeProbe->property(role).value<QColor>()
                                 : QColor();
}

void PowerSupplyMeterTest::knownChargeDrawsABoundAccentMeter() {
  auto *section = createSection(
      {batteryRow(QStringLiteral("supply-41-1"), true, 37.0, 1)});
  QVERIFY(section != nullptr);
  auto *meter = findItem(section, QStringLiteral("powerSupplyMeter_supply-41-1"));
  QVERIFY(meter != nullptr);
  QVERIFY(meter->isVisible());
  QVERIFY(meter->width() > 0 && meter->height() > 0);
  QCOMPARE(meter->property("from").toReal(), 0.0);
  QCOMPARE(meter->property("to").toReal(), 100.0);
  QCOMPARE(meter->property("value").toReal(), 37.0);
  QVERIFY(themeColour("accent").isValid());
  QCOMPARE(meter->property("color").value<QColor>(), themeColour("accent"));

  // The text still states the number; the bar only repeats it.
  auto *state = findItem(section, QStringLiteral("powerSupplyState_supply-41-1"));
  QVERIFY(state != nullptr);
  QVERIFY(state->property("text").toString().contains(QStringLiteral("37 percent")));

  auto *accessible = QAccessible::queryAccessibleInterface(meter);
  QVERIFY(accessible != nullptr);
  QCOMPARE(accessible->role(), QAccessible::ProgressBar);
  QCOMPARE(accessible->text(QAccessible::Name),
           QStringLiteral("Main battery charge bar, 37 percent"));

  // Bound, not copied: a republished reading moves the bar.
  m_model->supplyRows = {batteryRow(QStringLiteral("supply-41-1"), true, 64.0, 1)};
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  meter = findItem(section, QStringLiteral("powerSupplyMeter_supply-41-1"));
  QVERIFY(meter != nullptr);
  QCOMPARE(meter->property("value").toReal(), 64.0);
}

void PowerSupplyMeterTest::unknownChargeAndTheAdapterDrawNoMeter() {
  StubPowerSettingsModel defaults;
  QVariantList rows = defaults.supplyRows; // the AC adapter row
  rows.append(batteryRow(QStringLiteral("supply-41-1"), false, 0.0, 3));
  auto *section = createSection(rows);
  QVERIFY(section != nullptr);
  auto *battery = findItem(section, QStringLiteral("powerSupplyMeter_supply-41-1"));
  auto *adapter = findItem(section, QStringLiteral("powerSupplyMeter_ac-adapter"));
  // Unknown is not zero: no bar is drawn at all, not an empty one.
  QVERIFY(battery == nullptr || !battery->isVisible());
  QVERIFY(adapter == nullptr || !adapter->isVisible());
  auto *state = findItem(section, QStringLiteral("powerSupplyState_supply-41-1"));
  QVERIFY(state != nullptr);
  QVERIFY(state->property("text").toString().contains(QStringLiteral("Level Low")));

  // A later known reading brings the meter in.
  rows.last() = batteryRow(QStringLiteral("supply-41-1"), true, 12.0, 3);
  m_model->supplyRows = rows;
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  battery = findItem(section, QStringLiteral("powerSupplyMeter_supply-41-1"));
  QVERIFY(battery != nullptr);
  QVERIFY(battery->isVisible());
  QCOMPARE(battery->property("value").toReal(), 12.0);
}

void PowerSupplyMeterTest::onlyAPowerWarningChangesTheMeterColour() {
  auto *section = createSection({
      batteryRow(QStringLiteral("discharging"), true, 80.0, 2),
      batteryRow(QStringLiteral("low"), true, 9.0, 3),
      batteryRow(QStringLiteral("critical"), true, 4.0, 4),
      batteryRow(QStringLiteral("action"), true, 2.0, 5),
  });
  QVERIFY(section != nullptr);
  const auto colourOf = [section](const char *id) {
    auto *meter = findItem(section, QStringLiteral("powerSupplyMeter_")
                                        + QLatin1StringView(id));
    return meter != nullptr ? meter->property("color").value<QColor>() : QColor();
  };
  QVERIFY(themeColour("warning") != themeColour("accent"));
  QVERIFY(themeColour("danger") != themeColour("warning"));
  QCOMPARE(colourOf("discharging"), themeColour("accent"));
  QCOMPARE(colourOf("low"), themeColour("warning"));
  QCOMPARE(colourOf("critical"), themeColour("danger"));
  QCOMPARE(colourOf("action"), themeColour("danger"));
  // The warning is always also stated in words beside the bar.
  auto *warning = findItem(section, QStringLiteral("powerSupplyWarning_low"));
  QVERIFY(warning != nullptr);
  QVERIFY(warning->isVisible());
  QVERIFY(!warning->property("text").toString().isEmpty());
}

void PowerSupplyMeterTest::metersShareOneColumnAcrossRows() {
  // Every row is its own grid, and these names and state lines differ widely
  // in length. Only equal column shares keep the bars in one column.
  const QStringList ids{QStringLiteral("short"), QStringLiteral("long"),
                        QStringLiteral("full")};
  auto *section = createSection({
      batteryRow(ids.at(0), true, 76.0, 2, QStringLiteral("UPS"),
                 QStringLiteral("Charging")),
      batteryRow(ids.at(1), true, 9.0, 3,
                 QStringLiteral("Vendor Model Extended Life Battery Pack"),
                 QStringLiteral("Waiting to discharge")),
      batteryRow(ids.at(2), true, 100.0, 1, QStringLiteral("Main battery"),
                 QStringLiteral("Fully charged")),
  });
  QVERIFY(section != nullptr);
  // Each meter's (x mapped to the section, width); empty until all are shown.
  const auto geometry = [section, &ids] {
    QList<QPointF> placed;
    for (const QString &id : ids) {
      auto *meter = findItem(section, QStringLiteral("powerSupplyMeter_") + id);
      if (meter == nullptr || !meter->isVisible()) return QList<QPointF>{};
      placed.append({meter->mapToItem(section, QPointF(0, 0)).x(), meter->width()});
    }
    return placed;
  };
  const auto describe = [&geometry] {
    QStringList text;
    for (const QPointF &entry : geometry())
      text.append(QStringLiteral("x=%1 width=%2").arg(entry.x()).arg(entry.y()));
    return text.join(QStringLiteral("; "));
  };
  // Wide two-column rows, then narrower two-column rows (a row switches to
  // one column below 500 px). The meters sit in the second column, so a
  // settled layout puts them past 40 % of the width and inside the section;
  // a stale layout from the previous width fails one of the two.
  for (const qreal width : {720.0, 540.0}) {
    section->setWidth(width);
    const auto aligned = [&geometry, &ids, width] {
      const QList<QPointF> placed = geometry();
      if (placed.size() != ids.size()) return false;
      for (const QPointF &entry : placed) {
        if (entry != placed.first()) return false;
      }
      const QPointF first = placed.first();
      return first.y() > 0 && first.x() > width * 0.4
          && first.x() + first.y() <= width;
    };
    QTRY_VERIFY2(aligned(), qPrintable(describe()));
  }
}

QTEST_MAIN(PowerSupplyMeterTest)
#include "tst_power_supply_meter.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_qml_teardown.h"

#include <QQmlApplicationEngine>
#include <QtTest>

using namespace QindaQt::Apps::SettingsCenter;

namespace {

// A model the way Settings hands them to QML: a QObject owned by the
// composition root, read by bindings and by Component.onDestruction.
class CountingModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(int counter READ counter NOTIFY counterChanged)

public:
  [[nodiscard]] int counter() const { return m_counter; }

Q_SIGNALS:
  void counterChanged();

private:
  int m_counter = 7;
};

// Records what the QML scene could still read from its model at the moment the
// scene was destroyed. It is declared before the engine in every test so it
// outlives both the scene and the model; observing through a probe instead of
// parsing warning text keeps the assertion independent of Qt's message wording.
class DestructionProbe final : public QObject {
  Q_OBJECT

public:
  Q_INVOKABLE void record(bool modelWasReadable) {
    ++m_destructions;
    m_modelWasReadable = modelWasReadable;
  }
  [[nodiscard]] int destructions() const { return m_destructions; }
  [[nodiscard]] bool modelWasReadable() const { return m_modelWasReadable; }

private:
  int m_destructions = 0;
  bool m_modelWasReadable = false;
};

// A root that, like the Settings pages, reads its model in a binding and in
// its destruction handler.
constexpr const char *kRootQml = R"QML(
import QtQml
QtObject {
    required property var model
    required property var probe
    property int shown: model.counter
    Component.onDestruction: probe.record(!!(model && model.counter === 7))
}
)QML";

QStringList g_messages;
QtMessageHandler g_previousHandler = nullptr;
void collectMessages(QtMsgType type, const QMessageLogContext &, const QString &text) {
  if (type == QtWarningMsg || type == QtCriticalMsg) {
    g_messages.append(text);
  }
}

// QQmlApplicationEngine::loadData completes on the event loop, not inside the
// call, so the roots are awaited. Returns false (with the messages Qt printed)
// when the scene never appears, so a QML error cannot hide as an empty list.
bool loadScene(QQmlApplicationEngine &engine, const char *name) {
  engine.loadData(kRootQml, QUrl(QStringLiteral("inmemory:%1.qml").arg(QLatin1String(name))));
  return QTest::qWaitFor([&engine] { return !engine.rootObjects().isEmpty(); }, 5000);
}

void setInitialProperties(QQmlApplicationEngine &engine, CountingModel &model,
                          DestructionProbe &probe) {
  engine.setInitialProperties(
      {{QStringLiteral("model"), QVariant::fromValue(static_cast<QObject *>(&model))},
       {QStringLiteral("probe"), QVariant::fromValue(static_cast<QObject *>(&probe))}});
}

} // namespace

// AGENT-GUARD (ADR-0395): the QML scene must be destroyed before the models it
// reads. Plain scope exit does the opposite (engine declared first, models
// after), and every binding and destruction handler then reads an object that
// is gone.
class SettingsQmlTeardownTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void init() {
    g_messages.clear();
    g_previousHandler = qInstallMessageHandler(collectMessages);
  }
  void cleanup() { qInstallMessageHandler(g_previousHandler); }

  void theGuardDestroysTheSceneBeforeTheModels();
  void withoutTheGuardTheSceneOutlivesItsModel();
  void theGuardCoversAnEarlyReturn();
  void anAlreadyDeletedRootIsHarmless();
};

void SettingsQmlTeardownTests::theGuardDestroysTheSceneBeforeTheModels() {
  DestructionProbe probe;
  QStringList order;
  {
    QQmlApplicationEngine engine;
    CountingModel model;
    setInitialProperties(engine, model, probe);
    // Declared after the model, exactly as in main().
    QmlTeardown guard(engine);
    QVERIFY2(loadScene(engine, "teardown"), qPrintable(g_messages.join(QLatin1Char('\n'))));
    QObject::connect(engine.rootObjects().constFirst(), &QObject::destroyed,
                     [&order] { order.append(QStringLiteral("root")); });
    QObject::connect(&model, &QObject::destroyed,
                     [&order] { order.append(QStringLiteral("model")); });
  }
  QCOMPARE(order, (QStringList{QStringLiteral("root"), QStringLiteral("model")}));
  QCOMPARE(probe.destructions(), 1);
  QVERIFY2(probe.modelWasReadable(), "the scene's destruction handler lost its model");
  // Not a single binding or handler ran against a destroyed model.
  QVERIFY2(g_messages.isEmpty(), qPrintable(g_messages.join(QLatin1Char('\n'))));
}

void SettingsQmlTeardownTests::withoutTheGuardTheSceneOutlivesItsModel() {
  // The negative control: the same composition without the guard destroys the
  // model while the root is alive, so the root's destruction handler reads a
  // model that is gone. This is the defect the guard exists for; if this row
  // ever stops reproducing it, the guard row above proves nothing.
  DestructionProbe probe;
  QStringList order;
  {
    QQmlApplicationEngine engine;
    CountingModel model;
    setInitialProperties(engine, model, probe);
    QVERIFY2(loadScene(engine, "no-guard"), qPrintable(g_messages.join(QLatin1Char('\n'))));
    QObject::connect(&model, &QObject::destroyed,
                     [&order] { order.append(QStringLiteral("model")); });
    QObject::connect(engine.rootObjects().constFirst(), &QObject::destroyed,
                     [&order] { order.append(QStringLiteral("root")); });
  }
  QCOMPARE(order, (QStringList{QStringLiteral("model"), QStringLiteral("root")}));
  QCOMPARE(probe.destructions(), 1);
  QVERIFY2(!probe.modelWasReadable(),
           "without the guard the destruction handler must find its model destroyed");
}

void SettingsQmlTeardownTests::theGuardCoversAnEarlyReturn() {
  // An early return after the scene is loaded (the probe path, a failed
  // loadFromModule check) leaves through the same destructor.
  DestructionProbe probe;
  QStringList order;
  const auto composeAndLeave = [&probe, &order]() {
    QQmlApplicationEngine engine;
    CountingModel model;
    setInitialProperties(engine, model, probe);
    QmlTeardown guard(engine);
    if (!loadScene(engine, "early")) {
      return 3;
    }
    QObject::connect(engine.rootObjects().constFirst(), &QObject::destroyed,
                     [&order] { order.append(QStringLiteral("root")); });
    QObject::connect(&model, &QObject::destroyed,
                     [&order] { order.append(QStringLiteral("model")); });
    return 0; // the early return path
  };
  QCOMPARE(composeAndLeave(), 0);
  QCOMPARE(order, (QStringList{QStringLiteral("root"), QStringLiteral("model")}));
  QVERIFY(probe.modelWasReadable());
  QVERIFY2(g_messages.isEmpty(), qPrintable(g_messages.join(QLatin1Char('\n'))));
}

void SettingsQmlTeardownTests::anAlreadyDeletedRootIsHarmless() {
  // The application may delete a root itself (the route probe used to); the
  // engine keeps weak pointers, so the guard neither double-deletes nor trips.
  DestructionProbe probe;
  {
    QQmlApplicationEngine engine;
    CountingModel model;
    setInitialProperties(engine, model, probe);
    QmlTeardown guard(engine);
    QVERIFY2(loadScene(engine, "deleted"), qPrintable(g_messages.join(QLatin1Char('\n'))));
    delete engine.rootObjects().constFirst();
    QVERIFY(engine.rootObjects().isEmpty());
    QCOMPARE(probe.destructions(), 1);
  }
  // Exactly one destruction: the guard did not delete or notify a second time.
  QCOMPARE(probe.destructions(), 1);
  QVERIFY(probe.modelWasReadable());
  QVERIFY2(g_messages.isEmpty(), qPrintable(g_messages.join(QLatin1Char('\n'))));
}

QTEST_GUILESS_MAIN(SettingsQmlTeardownTests)
#include "tst_settings_qml_teardown.moc"

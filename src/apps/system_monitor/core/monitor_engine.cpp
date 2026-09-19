// SPDX-License-Identifier: LGPL-3.0-or-later

#include "monitor_engine.h"

#include "process_actions.h"
#include "process_model.h"
#include "sample_collector.h"

#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrentRun>

#include <algorithm>
#include <memory>

namespace QindaQt::SystemMonitor {
namespace {

struct CollectionResult final {
  PublishedSample sample;
  QString error;
};

} // namespace

class MonitorEngine::Private final {
public:
  explicit Private(MonitorEngine *engine)
      : owner(engine), model(engine),
        collector(std::make_shared<SampleCollector>()) {
    timer.setInterval(interval);
    timer.setTimerType(Qt::CoarseTimer);
    QObject::connect(&timer, &QTimer::timeout, engine, [this] { request(); });
    QObject::connect(&watcher, &QFutureWatcher<CollectionResult>::finished,
                     engine, [this] { finish(); });
    timer.start();
    QTimer::singleShot(0, engine, [this] { request(); });
  }

  void request() {
    if (watcher.isRunning()) {
      pending = true;
      return;
    }
    const auto retainedCollector = collector;
    watcher.setFuture(QtConcurrent::run([retainedCollector] {
      CollectionResult result;
      result.sample = retainedCollector->collect(&result.error);
      return result;
    }));
  }

  void finish() {
    const CollectionResult result = watcher.result();
    if (result.error.isEmpty()) {
      snapshot = result.sample.snapshot;
      model.replace(result.sample.processes);
      Q_EMIT owner->updated();
    } else {
      Q_EMIT owner->errorOccurred(result.error);
    }
    if (pending) {
      pending = false;
      request();
    }
  }

  MonitorEngine *owner;
  ProcessModel model;
  QTimer timer;
  QFutureWatcher<CollectionResult> watcher;
  std::shared_ptr<SampleCollector> collector;
  QVariantMap snapshot;
  int interval = 1000;
  bool paused = false;
  bool pending = false;
};

MonitorEngine::MonitorEngine(QObject *parent)
    : QObject(parent), d(new Private(this)) {}

MonitorEngine::~MonitorEngine() { delete d; }

QAbstractItemModel *MonitorEngine::processes() const { return &d->model; }

QVariantMap MonitorEngine::snapshot() const { return d->snapshot; }

int MonitorEngine::interval() const { return d->interval; }

bool MonitorEngine::paused() const { return d->paused; }

void MonitorEngine::setInterval(int milliseconds) {
  const int bounded = std::clamp(milliseconds, 250, 10000);
  if (d->interval == bounded) {
    return;
  }
  d->interval = bounded;
  d->timer.setInterval(bounded);
  Q_EMIT intervalChanged();
}

void MonitorEngine::setPaused(bool paused) {
  if (d->paused == paused) {
    return;
  }
  d->paused = paused;
  if (paused) {
    d->timer.stop();
    d->pending = false;
  } else {
    d->timer.start();
    d->request();
  }
  Q_EMIT pausedChanged();
}

void MonitorEngine::requestSample() { d->request(); }

QString MonitorEngine::processAction(qint64 pid, quint64 startTicks,
                                     const QString &action, int value) {
  const QString error = applyProcessAction(QStringLiteral("/proc"), pid,
                                           startTicks, action, value);
  if (!error.isEmpty()) {
    Q_EMIT errorOccurred(error);
  } else {
    requestSample();
  }
  return error;
}

} // namespace QindaQt::SystemMonitor

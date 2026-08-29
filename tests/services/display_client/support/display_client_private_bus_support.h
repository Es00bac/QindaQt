// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_service/display_service_ports.h>

#include <QtCore/QElapsedTimer>

namespace QindaQt::DisplayClient::TestSupport {

class ElapsedClock final : public DisplayTransaction::MonotonicClock {
public:
  ElapsedClock() { m_elapsed.start(); }
  [[nodiscard]] quint64 nowMilliseconds() const noexcept override {
    return static_cast<quint64>(m_elapsed.elapsed());
  }

private:
  QElapsedTimer m_elapsed;
};

class FakeTransactionPort final : public DisplayService::TransactionPort {
public:
  void setObserver(DisplayService::TransactionPortObserver *value) override {
    observer = value;
  }
  void beginMachineLineage(quint64 value) override { lineage = value; }
  DisplayTransaction::JournalMutationOutcome
  storeJournal(const DisplayTransaction::Journal &journal) override {
    journals.push_back(journal);
    return DisplayTransaction::JournalMutationOutcome::Durable;
  }
  DisplayTransaction::JournalMutationOutcome clearJournal() override {
    return DisplayTransaction::JournalMutationOutcome::Durable;
  }
  void requestApply(const DisplayTransaction::ApplyRequest &request) override {
    requestLineages.push_back(lineage);
    requests.push_back(request);
  }
  void completeLast(DisplayTransaction::ApplyOutcome outcome) {
    Q_ASSERT(observer != nullptr);
    observer->applyCompleted(requestLineages.constLast(),
                             requests.constLast().token, outcome);
  }

  DisplayService::TransactionPortObserver *observer = nullptr;
  QList<DisplayTransaction::Journal> journals;
  QList<DisplayTransaction::ApplyRequest> requests;
  QList<quint64> requestLineages;
  quint64 lineage = 0;
};

class FakeInventorySource final : public DisplayService::InventorySource {
public:
  void setObserver(DisplayService::InventoryObserver *value) override {
    observer = value;
  }
  DisplayService::InventorySourceStartStatus start() override {
    started = true;
    return DisplayService::InventorySourceStartStatus::Started;
  }
  void stop() override { started = false; }
  void publish(const DisplayService::InventoryFrame &frame) {
    Q_ASSERT(observer != nullptr);
    observer->inventoryObserved(frame);
  }

  DisplayService::InventoryObserver *observer = nullptr;
  bool started = false;
};

inline DisplayService::InventoryOutput
inventoryOutput(Display::Transform transform = Display::Transform::Normal) {
  return {.name = QStringLiteral("DP-1"),
          .geometry = QRect(0, 0, 1920, 1080),
          .scale = 1.0,
          .refreshRateMilliHertz = 60'000,
          .transform = transform,
          .internal = false,
          .runtimeCompositorUuid = QStringLiteral("runtime-uuid"),
          .compositorPriority = 0,
          .physicalSizeMillimeters = QSize(600, 340),
          .manufacturer = QStringLiteral("Qinda"),
          .model = QStringLiteral("Reference Display")};
}

inline DisplayService::InventoryFrame
inventoryFrame(quint64 generation,
               Display::Transform transform = Display::Transform::Normal,
               QString owner = QStringLiteral(":1.42")) {
  return {.uniqueOwner = std::move(owner),
          .outputGeneration = generation,
          .outputs = {inventoryOutput(transform)}};
}

} // namespace QindaQt::DisplayClient::TestSupport

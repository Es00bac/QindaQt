// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_secret_agent/qml_prompt_presenter.h>

#include <QtCore/QVariantList>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>

#include <utility>

namespace QindaQt::Network::SecretAgent {

QmlPromptPresenter::QmlPromptPresenter(QQmlEngine &engine, QObject *parent)
    : PromptPort(parent), m_engine(engine) {}

QmlPromptPresenter::~QmlPromptPresenter() {
  const QList<quint64> ids = m_active.keys();
  for (const quint64 requestId : ids) {
    cancelPrompt(requestId);
  }
}

bool QmlPromptPresenter::showPrompt(const PromptRequest &request,
                                    Completion completion) {
  if (request.requestId == 0 || request.fields.isEmpty() ||
      m_active.contains(request.requestId)) {
    return false;
  }
  QVariantList fields;
  fields.reserve(request.fields.size());
  for (const PromptField &field : request.fields) {
    fields.append(
        QVariantMap{{QStringLiteral("key"), field.key},
                    {QStringLiteral("label"), field.label},
                    {QStringLiteral("concealed"), field.concealed},
                    {QStringLiteral("maximumLength"), field.maximumLength}});
  }
  QQmlComponent component(
      &m_engine,
      QUrl(QStringLiteral(
          "qrc:/qindaqt/network-secret-agent/NetworkSecretPrompt.qml")));
  if (!component.isReady()) {
    return false;
  }
  QObject *window = component.createWithInitialProperties(
      {{QStringLiteral("requestId"), QVariant::fromValue(request.requestId)},
       {QStringLiteral("connectionName"), request.connectionName},
       {QStringLiteral("fieldSpecs"), fields},
       {QStringLiteral("presenter"),
        QVariant::fromValue(static_cast<QObject *>(this))}});
  if (window == nullptr) {
    return false;
  }
  m_active.insert(request.requestId,
                  ActivePrompt{window, request, std::move(completion)});
  window->setParent(this);
  return true;
}

void QmlPromptPresenter::cancelPrompt(const quint64 requestId) {
  closeAndForget(requestId);
}

void QmlPromptPresenter::submit(const qulonglong requestId,
                                const QVariantMap &values,
                                const bool remember) {
  auto it = m_active.find(requestId);
  if (it == m_active.end()) {
    return;
  }
  ActivePrompt active = it.value();
  m_active.erase(it);
  PromptResult result;
  result.requestId = requestId;
  result.accepted = true;
  result.remember = remember;
  for (const PromptField &field : std::as_const(active.request.fields)) {
    const QString text = values.value(field.key).toString();
    result.values.append({field.key, text.toUtf8()});
  }
  if (active.window != nullptr) {
    active.window->deleteLater();
  }
  active.completion(std::move(result));
}

void QmlPromptPresenter::cancelByUser(const qulonglong requestId) {
  auto it = m_active.find(requestId);
  if (it == m_active.end()) {
    return;
  }
  ActivePrompt active = it.value();
  m_active.erase(it);
  if (active.window != nullptr) {
    active.window->deleteLater();
  }
  PromptResult result;
  result.requestId = requestId;
  active.completion(std::move(result));
}

void QmlPromptPresenter::closeAndForget(const quint64 requestId) {
  auto it = m_active.find(requestId);
  if (it == m_active.end()) {
    return;
  }
  QObject *window = it->window;
  m_active.erase(it);
  if (window != nullptr) {
    QMetaObject::invokeMethod(window, "wipeAndClose");
    window->deleteLater();
  }
}

} // namespace QindaQt::Network::SecretAgent

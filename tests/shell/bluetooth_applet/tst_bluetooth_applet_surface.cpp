// SPDX-License-Identifier: GPL-3.0-or-later

#include "bluetooth_applet_controller.h"

#include "support/fake_bluetooth_transport.h"

#include <QMetaEnum>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QtTest>

#include <array>
#include <memory>
#include <span>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_BluetoothAppletPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt;
using namespace QindaQt::Shell::BluetoothApplet;
using namespace QindaQt::Tests;

namespace
{

struct PropertyContract {
    const char *name;
    const char *typeName;
    bool readable;
    bool writable;
    bool resettable;
    const char *notifySignature;
    bool constant;
    bool final;
};

struct MethodContract {
    const char *signature;
    const char *returnType;
    QMetaMethod::MethodType methodType;
    QMetaMethod::Access access;
    int revision;
};

struct EnumeratorContract {
    const char *name;
    const char *enumName;
    const char *scope;
    bool flag;
    bool scoped;
    const char *keysAndValues;
};

struct SurfaceContract {
    std::span<const PropertyContract> properties;
    std::span<const MethodContract> methods;
    std::span<const EnumeratorContract> enumerators;
};

constexpr std::array<PropertyContract, 17> kProperties{{
    {"phase", "QString", true, false, false, "stateChanged()", false, false},
    {"diagnostic", "QString", true, false, false, "stateChanged()", false, false},
    {"summaryLabel", "QString", true, false, false, "stateChanged()", false, false},
    {"accessibleName", "QString", true, false, false, "stateChanged()", false, false},
    {"accessibleDescription", "QString", true, false, false, "stateChanged()", false, false},
    {"serviceEpoch", "qulonglong", true, false, false, "stateChanged()", false, false},
    {"serviceRevision", "qulonglong", true, false, false, "stateChanged()", false, false},
    {"adapterRows", "QVariantList", true, false, false, "stateChanged()", false, false},
    {"deviceRows", "QVariantList", true, false, false, "stateChanged()", false, false},
    {"operationPending", "bool", true, false, false, "stateChanged()", false, false},
    {"discoveryLeaseHeld", "bool", true, false, false, "stateChanged()", false, false},
    {"feedbackPresent", "bool", true, false, false, "feedbackChanged()", false, false},
    {"feedback", "QString", true, false, false, "feedbackChanged()", false, false},
    {"pairingPromptVisible", "bool", true, false, false, "stateChanged()", false, false},
    {"pairingPromptText", "QString", true, false, false, "stateChanged()", false, false},
    {"pairingConfirmationAvailable", "bool", true, false, false, "stateChanged()", false, false},
    {"pairingReplyPending", "bool", true, false, false, "stateChanged()", false, false},
}};

constexpr std::array<MethodContract, 9> kMethods{{
    {"stateChanged()", "void", QMetaMethod::Signal, QMetaMethod::Public, 0},
    {"feedbackChanged()", "void", QMetaMethod::Signal, QMetaMethod::Public, 0},
    {"setExpanded(bool)", "void", QMetaMethod::Method, QMetaMethod::Public, 0},
    {"requestAdapterPower(QString,bool)", "bool", QMetaMethod::Method,
     QMetaMethod::Public, 0},
    {"requestDiscovery(QString,bool)", "bool", QMetaMethod::Method,
     QMetaMethod::Public, 0},
    {"requestDeviceConnection(QString,bool)", "bool", QMetaMethod::Method,
     QMetaMethod::Public, 0},
    {"confirmPrompt()", "bool", QMetaMethod::Method, QMetaMethod::Public, 0},
    {"cancelPrompt()", "bool", QMetaMethod::Method, QMetaMethod::Public, 0},
    {"clearFeedback()", "void", QMetaMethod::Method, QMetaMethod::Public, 0},
}};

constexpr std::array<EnumeratorContract, 0> kEnumerators{};
constexpr SurfaceContract kControllerSurface{kProperties, kMethods, kEnumerators};
constexpr SurfaceContract kEmptySurface{};

QString methodTypeName(const QMetaMethod::MethodType type)
{
    switch (type) {
    case QMetaMethod::Method:
        return QStringLiteral("Method");
    case QMetaMethod::Signal:
        return QStringLiteral("Signal");
    case QMetaMethod::Slot:
        return QStringLiteral("Slot");
    case QMetaMethod::Constructor:
        return QStringLiteral("Constructor");
    }
    return QStringLiteral("Unknown");
}

QString accessName(const QMetaMethod::Access access)
{
    switch (access) {
    case QMetaMethod::Private:
        return QStringLiteral("Private");
    case QMetaMethod::Protected:
        return QStringLiteral("Protected");
    case QMetaMethod::Public:
        return QStringLiteral("Public");
    }
    return QStringLiteral("Unknown");
}

QString describeProperty(const PropertyContract &property)
{
    return QString::fromLatin1("%1|%2|r=%3|w=%4|x=%5|notify=%6|constant=%7|final=%8")
        .arg(QString::fromLatin1(property.name), QString::fromLatin1(property.typeName))
        .arg(property.readable).arg(property.writable).arg(property.resettable)
        .arg(QString::fromLatin1(property.notifySignature))
        .arg(property.constant).arg(property.final);
}

QString describeProperty(const QMetaProperty &property)
{
    const QByteArray notify = property.hasNotifySignal()
        ? property.notifySignal().methodSignature() : QByteArray{};
    const PropertyContract actual{property.name(), property.typeName(),
                                  property.isReadable(), property.isWritable(),
                                  property.isResettable(), notify.constData(),
                                  property.isConstant(), property.isFinal()};
    return describeProperty(actual);
}

QString describeMethod(const MethodContract &method)
{
    return QString::fromLatin1("%1|return=%2|type=%3|access=%4|revision=%5")
        .arg(QString::fromLatin1(method.signature), QString::fromLatin1(method.returnType),
             methodTypeName(method.methodType), accessName(method.access))
        .arg(method.revision);
}

QString describeMethod(const QMetaMethod &method)
{
    const QByteArray signature = QMetaObject::normalizedSignature(
        method.methodSignature().constData());
    const MethodContract actual{signature.constData(), method.typeName(),
                                method.methodType(), method.access(),
                                method.revision()};
    return describeMethod(actual);
}

QString enumKeysAndValues(const QMetaEnum &enumerator)
{
    QStringList entries;
    for (int index = 0; index < enumerator.keyCount(); ++index) {
        entries.append(QString::fromLatin1(enumerator.key(index)) + u'='
                       + QString::number(enumerator.value(index)));
    }
    return entries.join(u',');
}

QString describeEnumerator(const EnumeratorContract &enumerator)
{
    return QString::fromLatin1("%1|enum=%2|scope=%3|flag=%4|scoped=%5|keys=%6")
        .arg(QString::fromLatin1(enumerator.name),
             QString::fromLatin1(enumerator.enumName),
             QString::fromLatin1(enumerator.scope))
        .arg(enumerator.flag).arg(enumerator.scoped)
        .arg(QString::fromLatin1(enumerator.keysAndValues));
}

QString describeEnumerator(const QMetaEnum &enumerator)
{
    const QByteArray keys = enumKeysAndValues(enumerator).toLatin1();
    const EnumeratorContract actual{enumerator.name(), enumerator.enumName(),
                                    enumerator.scope(), enumerator.isFlag(),
                                    enumerator.isScoped(), keys.constData()};
    return describeEnumerator(actual);
}

template<typename Contract, typename Describe>
QStringList expectedDescriptions(const std::span<const Contract> entries,
                                 Describe describe)
{
    QStringList descriptions;
    descriptions.reserve(static_cast<qsizetype>(entries.size()));
    for (const Contract &entry : entries) {
        descriptions.append(describe(entry));
    }
    return descriptions;
}

QString surfaceMismatch(const QMetaObject &metaObject,
                        const SurfaceContract &contract)
{
    QStringList actualProperties;
    for (int index = metaObject.propertyOffset(); index < metaObject.propertyCount(); ++index) {
        actualProperties.append(describeProperty(metaObject.property(index)));
    }
    QStringList actualMethods;
    for (int index = metaObject.methodOffset(); index < metaObject.methodCount(); ++index) {
        actualMethods.append(describeMethod(metaObject.method(index)));
    }
    QStringList actualEnumerators;
    for (int index = metaObject.enumeratorOffset(); index < metaObject.enumeratorCount(); ++index) {
        actualEnumerators.append(describeEnumerator(metaObject.enumerator(index)));
    }

    QStringList mismatches;
    const auto compare = [&mismatches](const QString &kind, const QStringList &actual,
                                       const QStringList &expected) {
        if (actual != expected) {
            mismatches.append(kind + QStringLiteral(" expected [")
                              + expected.join(QStringLiteral("; "))
                              + QStringLiteral("] but found [")
                              + actual.join(QStringLiteral("; ")) + u']');
        }
    };
    compare(QStringLiteral("properties"), actualProperties,
            expectedDescriptions(contract.properties,
                                 [](const PropertyContract &entry) {
                                     return describeProperty(entry);
                                 }));
    compare(QStringLiteral("methods"), actualMethods,
            expectedDescriptions(contract.methods,
                                 [](const MethodContract &entry) {
                                     return describeMethod(entry);
                                 }));
    compare(QStringLiteral("enumerators"), actualEnumerators,
            expectedDescriptions(contract.enumerators,
                                 [](const EnumeratorContract &entry) {
                                     return describeEnumerator(entry);
                                 }));
    return mismatches.join(u'\n');
}

QStringList stringList(const QVariant &value)
{
    QStringList result;
    const QVariantList entries = value.toList();
    result.reserve(entries.size());
    for (const QVariant &entry : entries) {
        result.append(entry.toString());
    }
    return result;
}

QStringList controllerPropertyNames()
{
    QStringList names;
    for (const PropertyContract &property : kProperties) {
        names.append(QString::fromLatin1(property.name));
    }
    names.sort();
    return names;
}

QStringList controllerMethodNames()
{
    QStringList names;
    for (const MethodContract &method : kMethods) {
        names.append(QString::fromLatin1(method.signature).section(u'(', 0, 0));
    }
    names.sort();
    return names;
}

class EmptySurface : public QObject
{
    Q_OBJECT
};

#define QINDAQT_TEST_SURFACE_JOIN_IMPL(left, right) left##right
#define QINDAQT_TEST_SURFACE_JOIN(left, right) \
    QINDAQT_TEST_SURFACE_JOIN_IMPL(left, right)

class ExpandedSurface final : public EmptySurface
{
    Q_OBJECT
    // The rejected lexical gate never saw this post-preprocessing property.
    QINDAQT_TEST_SURFACE_JOIN(Q_, PROPERTY)(
        bool unexpected READ unexpected NOTIFY unexpectedChanged)

public:
    enum class UnexpectedMode { Off, On };
    Q_ENUM(UnexpectedMode)

    [[nodiscard]] bool unexpected() const noexcept { return false; }

public Q_SLOTS:
    void unexpectedSlot() {}

Q_SIGNALS:
    void unexpectedChanged();
};

#undef QINDAQT_TEST_SURFACE_JOIN
#undef QINDAQT_TEST_SURFACE_JOIN_IMPL

} // namespace


class BluetoothAppletSurfaceTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void compiledMetaObjectMatchesLiteralContract();
    void comparisonRejectsExpandedSurface();
    void qmlVisibleNamesMatchLiteralContract();
};

void BluetoothAppletSurfaceTests::compiledMetaObjectMatchesLiteralContract()
{
    // AGENT-GUARD: The post-moc meta-object is the surface authority; source
    // spelling cannot substitute because token pasting and public slots are valid.
    const QString mismatch = surfaceMismatch(
        BluetoothAppletController::staticMetaObject, kControllerSurface);
    QVERIFY2(mismatch.isEmpty(), qPrintable(mismatch));
}

void BluetoothAppletSurfaceTests::comparisonRejectsExpandedSurface()
{
    const QString baselineMismatch = surfaceMismatch(
        EmptySurface::staticMetaObject, kEmptySurface);
    QVERIFY2(baselineMismatch.isEmpty(), qPrintable(baselineMismatch));

    const QString mismatch = surfaceMismatch(
        ExpandedSurface::staticMetaObject, kEmptySurface);
    QVERIFY(mismatch.contains(QStringLiteral("properties")));
    QVERIFY(mismatch.contains(QStringLiteral("methods")));
    QVERIFY(mismatch.contains(QStringLiteral("enumerators")));
    QVERIFY(mismatch.contains(QStringLiteral("unexpected")));
    QVERIFY(mismatch.contains(QStringLiteral("unexpectedSlot")));
    QVERIFY(mismatch.contains(QStringLiteral("UnexpectedMode")));
}

void BluetoothAppletSurfaceTests::qmlVisibleNamesMatchLiteralContract()
{
    FakeBluetoothTransport transport;
    Bluetooth::BluetoothClient client(&transport);
    BluetoothAppletController controller(&client, true, true);
    QObject qObjectBaseline;

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(QINDAQT_BLUETOOTH_APPLET_QML_IMPORT_PATH));
    QQmlComponent component(&engine);
    component.setData(R"QML(
        import QtQuick
        import QindaQt.Shell.BluetoothApplet
        Item {
            id: root
            required property var access
            required property var qObjectReference
            property var accessProperties: []
            property var accessMethods: []
            property var baselineProperties: []
            property var baselineMethods: []
            function reflect(subject, methods) {
                const result = []
                for (const name in subject) {
                    if ((typeof subject[name] === "function") === methods)
                        result.push(name)
                }
                result.sort()
                return result
            }
            BluetoothApplet {
                access: root.access
                theme: ({ cornerRadius: 0, colors: ({}) })
            }
            Component.onCompleted: {
                accessProperties = reflect(access, false)
                accessMethods = reflect(access, true)
                baselineProperties = reflect(qObjectReference, false)
                baselineMethods = reflect(qObjectReference, true)
            }
        }
    )QML", QUrl(QStringLiteral("qrc:/qindaqt/tests/BluetoothSurface.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> instance(component.createWithInitialProperties(
        {{QStringLiteral("access"), QVariant::fromValue(&controller)},
         {QStringLiteral("qObjectReference"),
          QVariant::fromValue(&qObjectBaseline)}}));
    QVERIFY2(instance != nullptr, qPrintable(component.errorString()));

    QStringList properties = stringList(instance->property("accessProperties"));
    const QStringList baselineProperties = stringList(
        instance->property("baselineProperties"));
    for (const QString &name : baselineProperties) {
        properties.removeAll(name);
    }
    QStringList methods = stringList(instance->property("accessMethods"));
    const QStringList baselineMethods = stringList(instance->property("baselineMethods"));
    for (const QString &name : baselineMethods) {
        methods.removeAll(name);
    }
    QCOMPARE(properties, controllerPropertyNames());
    QCOMPARE(methods, controllerMethodNames());
}

QTEST_MAIN(BluetoothAppletSurfaceTests)
#include "tst_bluetooth_applet_surface.moc"

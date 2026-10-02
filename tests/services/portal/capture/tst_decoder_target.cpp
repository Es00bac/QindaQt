// SPDX-License-Identifier: GPL-3.0-or-later
#include "pipewire_frames.h"
#include <pipewire/extensions/metadata.h>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>
#include <cstring>
#include <functional>
#include <memory>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
namespace {
std::function<void(pw_stream *)> beforeConnect;
// This link seam belongs only to the dedicated graph fixture. Both the repaired
// decoder and byte-identical ca299 control call the actual PipeWire function.
// No portal/compositor authorization path or decoder implementation is replaced.
pw_stream *lastConsumer = nullptr;
}
extern "C" int __real_pw_stream_connect(pw_stream *, pw_direction, uint32_t, pw_stream_flags, const spa_pod **, uint32_t);
extern "C" int __wrap_pw_stream_connect(pw_stream *stream, pw_direction direction, uint32_t target,
                                         pw_stream_flags flags, const spa_pod **params, uint32_t count) {
    lastConsumer = stream;
    if (beforeConnect) { auto hook = std::move(beforeConnect); beforeConnect = {}; hook(stream); }
    return __real_pw_stream_connect(stream, direction, target, flags, params, count);
}
namespace {
class PrivateGraph final {
public:
    struct Node { QString name, serial; };
    PrivateGraph() {
        pw_init(nullptr, nullptr); loop = pw_loop_new(nullptr); if (!loop) return;
        pw_loop_enter(loop); context = pw_context_new(loop, nullptr, 0); if (!context) return;
        core = pw_context_connect(context, nullptr, 0); if (!core) return;
        static const pw_core_events coreEvents = [] {
            pw_core_events e{}; e.version = PW_VERSION_CORE_EVENTS;
            e.done = [](void *data, uint32_t id, int seq) { auto &g = *static_cast<PrivateGraph *>(data); if (id == PW_ID_CORE && seq == g.syncSequence) g.synced = true; };
            e.error = [](void *data, uint32_t, int, int, const char *text) { static_cast<PrivateGraph *>(data)->error = QString::fromUtf8(text); };
            return e;
        }();
        pw_core_add_listener(core, &coreListener, &coreEvents, this);
        registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0); if (!registry) return;
        static const pw_registry_events registryEvents = [] {
            pw_registry_events e{}; e.version = PW_VERSION_REGISTRY_EVENTS;
            e.global = [](void *data, uint32_t id, uint32_t, const char *type, uint32_t, const spa_dict *props) {
                auto &g = *static_cast<PrivateGraph *>(data); if (!props) return;
                const auto property = [props](const char *key) { return QString::fromUtf8(spa_dict_lookup(props, key)); };
                if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0) g.nodes.insert(id, {property(PW_KEY_NODE_NAME), property(PW_KEY_OBJECT_SERIAL)});
                if (strcmp(type, PW_TYPE_INTERFACE_Link) == 0) g.links.insert(id, {property(PW_KEY_LINK_INPUT_NODE).toUInt(), property(PW_KEY_LINK_OUTPUT_NODE).toUInt()});
                if (strcmp(type, PW_TYPE_INTERFACE_Metadata) == 0 && property(PW_KEY_METADATA_NAME) == "default" && !g.metadata) {
                    g.metadata = static_cast<pw_metadata *>(pw_registry_bind(g.registry, id, type, PW_VERSION_METADATA, 0));
                    if (g.metadata) pw_metadata_add_listener(g.metadata, &g.metadataListener, &metadataEvents(), &g);
                }
            };
            e.global_remove = [](void *data, uint32_t id) { auto &g = *static_cast<PrivateGraph *>(data); g.nodes.remove(id); g.links.remove(id); };
            return e;
        }();
        pw_registry_add_listener(registry, &registryListener, &registryEvents, this);
        timer.setInterval(5); QObject::connect(&timer, &QTimer::timeout, [&] { iterate(); }); timer.start();
    }
    ~PrivateGraph() {
        timer.stop(); beforeConnect = {}; lastConsumer = nullptr;
        if (offered) pw_proxy_destroy(offered);
        if (unrelated) pw_proxy_destroy(unrelated);
        if (metadata) { spa_hook_remove(&metadataListener); pw_proxy_destroy(reinterpret_cast<pw_proxy *>(metadata)); }
        if (registry) { spa_hook_remove(&registryListener); pw_proxy_destroy(reinterpret_cast<pw_proxy *>(registry)); }
        if (core) { spa_hook_remove(&coreListener); pw_core_disconnect(core); }
        if (context) pw_context_destroy(context);
        if (loop) { pw_loop_leave(loop); pw_loop_destroy(loop); }
    }
    bool valid() const { return registry && error.isEmpty(); }
    bool createSources() {
        offered = create("private-offered-video", 1); unrelated = create("private-unrelated-video", 2);
        return offered && unrelated && sync();
    }
    quint32 node(const QString &name) const { for (auto it = nodes.cbegin(); it != nodes.cend(); ++it) if (it->name == name) return it.key(); return PW_ID_ANY; }
    bool configureDefault() {
        if (!metadata) return false;
        const auto value = QJsonDocument(QJsonObject{{"name", "private-unrelated-video"}}).toJson(QJsonDocument::Compact);
        return pw_metadata_set_property(metadata, PW_ID_CORE, "default.configured.video.source", "Spa:String:JSON", value.constData()) >= 0;
    }
    bool retireOffered() { if (!offered) return false; pw_proxy_destroy(offered); offered = nullptr; return sync(); }
    quint32 linkedSource(quint32 consumer) const { for (const auto &link : links) if (link.first == consumer) return link.second; return PW_ID_ANY; }
    bool sync() {
        synced = false; syncSequence = pw_core_sync(core, PW_ID_CORE, 0); if (syncSequence < 0) return false;
        QElapsedTimer deadline; deadline.start();
        while (!synced && error.isEmpty() && deadline.elapsed() < 2000) if (pw_loop_iterate(loop, 10) < 0) return false;
        return synced && error.isEmpty();
    }
    void iterate() { if (loop && pw_loop_iterate(loop, 0) < 0) error = "private graph iteration failed"; }
    QString defaultVideo, error; QHash<quint32, Node> nodes;
private:
    static const pw_metadata_events &metadataEvents() {
        static const pw_metadata_events events = [] {
            pw_metadata_events e{}; e.version = PW_VERSION_METADATA_EVENTS;
            e.property = [](void *data, uint32_t subject, const char *key, const char *, const char *value) {
                if (subject == PW_ID_CORE && key && strcmp(key, "default.video.source") == 0)
                    static_cast<PrivateGraph *>(data)->defaultVideo = QJsonDocument::fromJson(value ? QByteArray(value) : QByteArray{}).object().value("name").toString();
                return 0;
            }; return e;
        }();
        return events;
    }
    pw_proxy *create(const char *name, int pattern) {
        auto *props = pw_properties_new(PW_KEY_FACTORY_NAME, "videotestsrc", PW_KEY_NODE_NAME, name,
            PW_KEY_MEDIA_CLASS, "Video/Source", "node.virtual", "true", nullptr);
        const auto parameter = QByteArray("{ patternType = ") + QByteArray::number(pattern) + " }";
        pw_properties_set(props, "node.param.Props", parameter.constData());
        auto *proxy = static_cast<pw_proxy *>(pw_core_create_object(core, "spa-node-factory", PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, &props->dict, 0));
        pw_properties_free(props); return proxy;
    }
    pw_loop *loop = nullptr; pw_context *context = nullptr; pw_core *core = nullptr;
    pw_registry *registry = nullptr; pw_metadata *metadata = nullptr; pw_proxy *offered = nullptr, *unrelated = nullptr;
    spa_hook coreListener{}, registryListener{}, metadataListener{}; QTimer timer;
    int syncSequence = -1; bool synced = false; QHash<quint32, QPair<quint32, quint32>> links;
};
int privateRemote() {
    const auto runtime = qEnvironmentVariable("PIPEWIRE_RUNTIME_DIR"), remote = qEnvironmentVariable("PIPEWIRE_REMOTE");
    if (runtime.isEmpty() || remote != "pipewire-decoder-private") return -1;
    const auto path = QFile::encodeName(runtime + '/' + remote); sockaddr_un address{}; address.sun_family = AF_UNIX;
    if (path.size() >= static_cast<qsizetype>(sizeof(address.sun_path))) return -1;
    memcpy(address.sun_path, path.constData(), static_cast<size_t>(path.size()) + 1);
    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0); if (fd < 0) return -1;
    if (::connect(fd, reinterpret_cast<const sockaddr *>(&address), sizeof(address))) { ::close(fd); return -1; }
    return fd;
}
}
class DecoderTargetTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init() {
        graph = std::make_unique<PrivateGraph>(); QVERIFY(graph->valid()); QVERIFY(graph->createSources());
        QTRY_VERIFY_WITH_TIMEOUT(graph->node("private-offered-video") != PW_ID_ANY && graph->node("private-unrelated-video") != PW_ID_ANY, 3000);
        offered = graph->node("private-offered-video"); unrelated = graph->node("private-unrelated-video"); QVERIFY(offered != unrelated);
        QTRY_VERIFY_WITH_TIMEOUT(graph->configureDefault(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(graph->defaultVideo, QStringLiteral("private-unrelated-video"), 3000);
        qInfo().noquote() << QString("actual graph offered=%1 serial=%2 unrelated-default=%3 serial=%4").arg(offered).arg(graph->nodes.value(offered).serial).arg(unrelated).arg(graph->nodes.value(unrelated).serial);
    }
    void cleanup() { beforeConnect = {}; graph.reset(); }
    void exactAvailableSource_data() { QTest::addColumn<bool>("useDefault"); QTest::newRow("explicit-offered") << false; QTest::newRow("explicit-unrelated-default") << true; }
    void exactAvailableSource() {
        QFETCH(bool, useDefault); const auto selected = useDefault ? unrelated : offered;
        const int fd = privateRemote(); QVERIFY(fd >= 0); PipeWireFrames frames(fd, selected); QVERIFY2(frames.valid(), qPrintable(frames.error()));
        QTRY_VERIFY2_WITH_TIMEOUT(frames.count() > 3, qPrintable(frames.error()), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(graph->linkedSource(pw_stream_get_node_id(lastConsumer)), selected, 3000);
        QVERIFY(frames.error().isEmpty()); QVERIFY(!frames.image().isNull());
    }
    void retiredOfferedTargetNeverUsesDefault() {
        const auto offeredSerial = graph->nodes.value(offered).serial; bool retired = false; QString resolved;
        beforeConnect = [&](pw_stream *stream) {
            resolved = QString::fromUtf8(pw_properties_get(pw_stream_get_properties(stream), PW_KEY_TARGET_OBJECT));
            retired = graph->retireOffered() && !graph->nodes.contains(offered) && graph->nodes.contains(unrelated);
            qInfo().noquote() << QString("actual pre-connect retirement resolved-serial=%1 retired=%2 default-still-present=%3").arg(resolved).arg(retired).arg(graph->nodes.contains(unrelated));
        };
        const int fd = privateRemote(); QVERIFY(fd >= 0); PipeWireFrames frames(fd, offered);
        QCOMPARE(resolved, offeredSerial); QVERIFY(retired); QVERIFY(graph->nodes.contains(unrelated));
        QTRY_VERIFY_WITH_TIMEOUT(!frames.error().isEmpty() || frames.count() > 0, 5000);
        if (frames.count() > 0) QTRY_COMPARE_WITH_TIMEOUT(graph->linkedSource(pw_stream_get_node_id(lastConsumer)), unrelated, 3000);
        qInfo().noquote() << QString("actual retired-target result frames=%1 error=%2 linked-source=%3 unrelated=%4").arg(frames.count()).arg(frames.error()).arg(graph->linkedSource(pw_stream_get_node_id(lastConsumer))).arg(unrelated);
        QCOMPARE(frames.count(), 0); QVERIFY(!frames.error().isEmpty());
        QVERIFY(graph->nodes.contains(unrelated)); QCOMPARE(graph->defaultVideo, QStringLiteral("private-unrelated-video"));
    }
private:
    std::unique_ptr<PrivateGraph> graph; quint32 offered = PW_ID_ANY, unrelated = PW_ID_ANY;
};
int main(int argc, char **argv) {
    rlimit cores{0, 0}; if (setrlimit(RLIMIT_CORE, &cores)) return 2;
    QCoreApplication app(argc, argv); DecoderTargetTest test; return QTest::qExec(&test, argc, argv);
}
#include "tst_decoder_target.moc"

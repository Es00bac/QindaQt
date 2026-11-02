// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import android.content.ComponentName;
import android.os.Build;
import android.view.Surface;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.HashSet;
import java.util.Set;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * One desktop session: hello, then control and inventory requests until the desktop leaves.
 *
 * <p>Ownership: the session owns the virtual display, the encoder and the injector, and
 * releases all three in {@link #close()} whatever ended the session.
 *
 * <p>AGENT-CONTRACT: message shapes follow docs/wiki/reference/qindalink-mirror-v1.md and
 * src/apps/link_mirror/src/mirror_messages.cpp on the desktop.
 */
final class Session implements ScreenEncoder.Sink {
    private final Connection connection;
    private final HiddenApi api;
    private final String token;
    private final VirtualDisplayHost display;
    private final InputInjector injector;
    private final AppLauncher launcher;
    private final AppInventory inventory = new AppInventory();
    private final DisplayPower power;
    private ScreenEncoder encoder;
    private int width;
    private int height;
    private int dpi;

    Session(Connection connection, HiddenApi api, String token) {
        this.connection = connection;
        this.api = api;
        this.token = token;
        this.display = new VirtualDisplayHost(api);
        this.injector = new InputInjector(api);
        this.launcher = new AppLauncher(api, 0);
        this.power = new DisplayPower(api);
    }

    /** Returns false when the peer failed the hello; the server then keeps listening. */
    boolean run() {
        try {
            if (!hello()) {
                return false;
            }
            Frames.Frame frame;
            while ((frame = connection.read()) != null) {
                if (!dispatch(frame)) {
                    break;
                }
            }
            Ln.i("session ended");
        } catch (Frames.ProtocolException e) {
            Ln.w("protocol error: " + e.getMessage());
        } catch (JSONException e) {
            sendError("bad-message", "malformed control body: " + e.getMessage(), true);
        } catch (IOException e) {
            if (!connection.isClosed()) {
                Ln.i("connection closed: " + Ln.describe(e));
            }
        } catch (Exception e) {
            Ln.e("session failed", e);
            sendError("internal", Ln.describe(e), true);
        } finally {
            close();
        }
        return true;
    }

    private boolean hello() throws IOException, JSONException {
        Frames.Frame frame = connection.read();
        if (frame == null || frame.channel != Frames.CH_CONTROL || frame.type != Frames.C_HELLO) {
            Ln.w("first frame was not hello; dropping peer");
            return false;
        }
        JSONObject body;
        try {
            body = json(frame);
        } catch (JSONException e) {
            Ln.w("hello was not JSON; dropping peer");
            return false;
        }
        if (!token.equals(body.optString("token"))) {
            Ln.w("hello token mismatch; dropping peer");
            return false;
        }
        if (body.optInt("version", -1) != Frames.PROTOCOL_VERSION) {
            sendError("unsupported-version", "server speaks version " + Frames.PROTOCOL_VERSION, true);
            throw new IOException("unsupported desktop version");
        }
        JSONObject ready = new JSONObject();
        ready.put("version", Frames.PROTOCOL_VERSION);
        ready.put("sdk", Build.VERSION.SDK_INT);
        ready.put("release", Build.VERSION.RELEASE);
        ready.put("model", Build.MODEL);
        ready.put("missing", new JSONArray(api.missing));
        connection.sendJson(Frames.CH_CONTROL, Frames.C_READY, ready);
        return true;
    }

    private boolean dispatch(Frames.Frame frame) throws Exception {
        if (frame.channel == Frames.CH_INVENTORY && frame.type == Frames.I_LIST) {
            listApps(json(frame));
            return true;
        }
        if (frame.channel != Frames.CH_CONTROL) {
            Ln.d("ignoring frame " + frame.channel + "/" + frame.type);
            return true;
        }
        JSONObject body = json(frame);
        switch (frame.type) {
            case Frames.C_START:
                start(body);
                return true;
            case Frames.C_RESIZE:
                resize(body.getInt("width"), body.getInt("height"), body.optInt("dpi", dpi));
                return true;
            case Frames.C_TOUCH:
                injector.touch(body.getString("action"), body.optInt("pointerId", 0),
                        (float) body.getDouble("x"), (float) body.getDouble("y"),
                        (float) body.optDouble("pressure", 1.0));
                return true;
            case Frames.C_MOUSE:
                injector.mouse(body.getString("action"), (float) body.getDouble("x"),
                        (float) body.getDouble("y"), body.optInt("buttons", 0));
                return true;
            case Frames.C_SCROLL:
                injector.scroll((float) body.getDouble("x"), (float) body.getDouble("y"),
                        (float) body.optDouble("h", 0), (float) body.optDouble("v", 0));
                return true;
            case Frames.C_KEY:
                injector.key(body.getString("action"), body.getInt("keycode"), body.optInt("meta", 0),
                        body.optInt("repeat", 0));
                return true;
            case Frames.C_TEXT:
                String text = body.getString("text");
                int unmapped = injector.text(text);
                if (unmapped > 0) {
                    Ln.d("text: " + unmapped + " of " + text.length() + " characters had no key mapping");
                }
                return true;
            case Frames.C_SYNC:
                if (encoder != null) {
                    encoder.requestSyncFrame();
                }
                return true;
            case Frames.C_STOP:
                return false;
            default:
                Ln.d("ignoring control type " + frame.type);
                return true;
        }
    }

    private void start(JSONObject body) throws Exception {
        if (encoder != null) {
            sendError("bad-state", "session already started", false);
            return;
        }
        if (!api.complete()) {
            sendError("api-missing", api.report(), true);
            throw new IOException("hidden API incomplete");
        }
        String pkg = body.getString("package");
        ComponentName component = launcher.resolveLauncher(pkg);
        if (component == null) {
            sendError("no-launcher-activity", "no launchable activity for the requested package", true);
            throw new IOException("unknown package");
        }
        String codec = body.optString("codec", "h264");
        if (!"h264".equals(codec) && !"hevc".equals(codec)) {
            sendError("bad-request", "codec must be h264 or hevc", true);
            throw new IOException("bad codec");
        }
        int bitrate = clamp(body.optInt("bitrateBps", 8_000_000), 500_000, 100_000_000);
        int fps = clamp(body.optInt("maxFps", 60), 1, 120);
        encoder = new ScreenEncoder(codec, bitrate, fps, this);
        dpi = clamp(body.getInt("dpi"), 72, 960);
        int[] size = encoder.fitSize(clamp(body.getInt("width"), 64, 8192), clamp(body.getInt("height"), 64, 8192));
        Surface surface = encoder.start(size[0], size[1]);
        width = size[0];
        height = size[1];
        int id = display.create(HiddenApi.SHELL_PACKAGE + ".qindalink-mirror", width, height, dpi, surface);
        injector.setDisplayId(id);
        power.keepAwake(id);
        launcher.launch(component, id);
        sendDisplay();
    }

    private void resize(int requestedWidth, int requestedHeight, int requestedDpi) throws Exception {
        if (encoder == null) {
            sendError("bad-state", "resize before start", false);
            return;
        }
        int[] size = encoder.fitSize(clamp(requestedWidth, 64, 8192), clamp(requestedHeight, 64, 8192));
        int newDpi = clamp(requestedDpi, 72, 960);
        if (size[0] != width || size[1] != height || newDpi != dpi) {
            // AGENT-GUARD: detach the surface before stopping its encoder; the display must
            // never draw into a released surface.
            display.setSurface(null);
            encoder.stop();
            Surface surface = encoder.start(size[0], size[1]);
            display.resize(size[0], size[1], newDpi);
            display.setSurface(surface);
            width = size[0];
            height = size[1];
            dpi = newDpi;
        }
        sendDisplay();
    }

    private void listApps(JSONObject body) throws Exception {
        Set<String> only = null;
        JSONArray packages = body.optJSONArray("packages");
        if (packages != null) {
            only = new HashSet<>();
            for (int i = 0; i < packages.length(); i++) {
                only.add(packages.getString(i));
            }
        }
        int count = 0;
        JSONObject end = new JSONObject();
        try {
            count = inventory.list(only, body.optInt("iconSize", 96),
                    app -> connection.sendJson(Frames.CH_INVENTORY, Frames.I_APP, app));
        } catch (IOException e) {
            throw e;
        } catch (Exception e) {
            // Inventory is presentation metadata; its failure must not end a running stream.
            Ln.e("inventory failed", e);
            end.put("error", Ln.describe(e));
        }
        connection.sendJson(Frames.CH_INVENTORY, Frames.I_END, end.put("count", count));
    }

    private void sendDisplay() throws IOException, JSONException {
        JSONObject message = new JSONObject();
        message.put("displayId", display.displayId());
        message.put("width", width);
        message.put("height", height);
        message.put("dpi", dpi);
        message.put("flags", display.flags());
        connection.sendJson(Frames.CH_CONTROL, Frames.C_DISPLAY, message);
    }

    private void sendError(String code, String message, boolean fatal) {
        try {
            JSONObject error = new JSONObject();
            error.put("code", code);
            error.put("message", message);
            error.put("fatal", fatal);
            connection.sendJson(Frames.CH_CONTROL, Frames.C_ERROR, error);
        } catch (IOException | JSONException e) {
            Ln.w("could not send error " + code);
        }
        Ln.w("error " + code + ": " + message);
    }

    private void close() {
        if (encoder != null) {
            display.setSurfaceQuietly();
            encoder.stop();
        }
        power.release();
        display.release();
        connection.close();
    }

    @Override
    public void onFormat(String codec, int formatWidth, int formatHeight) throws IOException {
        try {
            JSONObject format = new JSONObject();
            format.put("codec", codec);
            format.put("width", formatWidth);
            format.put("height", formatHeight);
            connection.sendJson(Frames.CH_VIDEO, Frames.V_FORMAT, format);
        } catch (JSONException e) {
            throw new IOException(e);
        }
    }

    @Override
    public void onPacket(int flags, long ptsUs, ByteBuffer data) throws IOException {
        connection.sendVideoPacket(flags, ptsUs, data);
    }

    @Override
    public void onEncoderFailure(Exception error) {
        Ln.e("encoder stopped", error);
        connection.close(); // ends the session loop; close() releases the display
    }

    private static JSONObject json(Frames.Frame frame) throws JSONException {
        return new JSONObject(new String(frame.payload, StandardCharsets.UTF_8));
    }

    private static int clamp(int value, int min, int max) {
        return Math.max(min, Math.min(max, value));
    }
}

package org.qindaqt.link.android;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;
import java.io.File;
import org.qindaqt.link.engine.DeviceInfo;
import org.qindaqt.link.engine.EngineState;
import org.qindaqt.link.engine.FeatureRegistry;
import org.qindaqt.link.engine.LinkEngine;
import org.qindaqt.link.engine.StatusFeature;
import org.qindaqt.link.engine.PinStore;

/**
 * Foreground service (type connectedDevice) that owns the {@link LinkEngine}.
 * The engine and its state live in static fields so the Qt UI bridge can
 * attach and detach freely; the engine keeps running with no UI.
 *
 * AGENT-GUARD: the engine must only be created here. Connection target is
 * 127.0.0.1:47160 unless the debug-only intent extra "port" overrides it
 * (the dev fake desktop uses 47199 via adb reverse so no override is needed).
 */
public final class LinkService extends Service {
    private static final String TAG = "QindaLink";
    private static final String CHANNEL = "link-status";
    static volatile LinkEngine engine;
    private static AndroidStatusSource statusSource;

    /** Starts the service; safe to call repeatedly. */
    public static void start(Context ctx) {
        Intent i = new Intent(ctx, LinkService.class);
        ctx.startForegroundService(i);
    }

    static LinkEngine engine() { return engine; }

    @Override public IBinder onBind(Intent intent) { return null; }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        startForeground(1, notification(), ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE);
        synchronized (LinkService.class) {
            if (engine == null) {
                try {
                    Identity id = Identity.loadOrCreate();
                    File dir = getFilesDir();
                    id.exportCertificatePem(new File(dir, "identity-cert.pem"));
                    DeviceInfo info = new DeviceInfo(Build.MODEL, Build.MODEL, "Android " + Build.VERSION.RELEASE, "0.1.0");
                    statusSource = new AndroidStatusSource(this);
                    statusSource.start();
                    EngineState state = new EngineState();
                    FeatureRegistry registry = new FeatureRegistry().add(new StatusFeature(statusSource, state));
                    engine = new LinkEngine(new TlsConnector(id, "127.0.0.1", 47160),
                            new PinStore(new File(dir, "desktop-pin")), registry, info, state);
                    engine.start();
                    Log.i(TAG, "engine started");
                } catch (Exception e) {
                    Log.e(TAG, "identity setup failed: " + e.getClass().getSimpleName());
                    stopSelf();
                }
            }
        }
        return START_STICKY;
    }

    @Override public void onDestroy() {
        synchronized (LinkService.class) {
            if (engine != null) { engine.stop(); engine = null; }
            if (statusSource != null) { statusSource.stop(); statusSource = null; }
        }
        super.onDestroy();
    }

    private Notification notification() {
        NotificationManager nm = getSystemService(NotificationManager.class);
        nm.createNotificationChannel(new NotificationChannel(CHANNEL, "QindaLink connection", NotificationManager.IMPORTANCE_LOW));
        return new Notification.Builder(this, CHANNEL)
                .setContentTitle("QindaLink")
                .setContentText("Looking for your desktop")
                .setSmallIcon(android.R.drawable.stat_notify_sync_noanim)
                .setOngoing(true)
                .build();
    }
}

package org.qindaqt.link.android;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioAttributes;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.RingtoneManager;
import android.net.Uri;
import org.qindaqt.link.ring.RingSink;

/**
 * Plays the find-my-phone alarm on the ALARM stream at full volume and posts a high-priority notification
 * with a Stop action and a full-screen intent. Ends on Stop, on unlock (USER_PRESENT) or when the owner
 * calls {@link #stop()}; restores the previous alarm volume.
 *
 * AGENT-GUARD: never exercised automatically on a real phone (user approval needed for a live ring). Alarm
 * stream playback is allowed through silent mode; Do Not Disturb is bypassed only if the user granted
 * notification-policy access, otherwise the note says "no-dnd-access" and the alarm plays anyway.
 */
final class RingSinkAndroid implements RingSink {
    private static final String CHANNEL = "link-ring";
    private static final String ACTION_STOP = "org.qindaqt.link.RING_STOP";
    private static final int NOTIF_ID = 7;

    private final Context ctx;
    private MediaPlayer player;
    private AudioManager audio;
    private int savedVolume = -1;
    private BroadcastReceiver rx;

    RingSinkAndroid(Context ctx) { this.ctx = ctx; }

    @Override public synchronized String start(Runnable onStopped) {
        stop();
        NotificationManager nm = ctx.getSystemService(NotificationManager.class);
        boolean dnd = nm.isNotificationPolicyAccessGranted();
        audio = (AudioManager) ctx.getSystemService(Context.AUDIO_SERVICE);
        try {
            savedVolume = audio.getStreamVolume(AudioManager.STREAM_ALARM);
            audio.setStreamVolume(AudioManager.STREAM_ALARM, audio.getStreamMaxVolume(AudioManager.STREAM_ALARM), 0);
        } catch (RuntimeException e) {
            savedVolume = -1; // volume not changeable here; ring at whatever it is
        }
        try {
            Uri tone = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM);
            if (tone == null) tone = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_RINGTONE);
            MediaPlayer p = new MediaPlayer();
            p.setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_ALARM)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build());
            p.setDataSource(ctx, tone);
            p.setLooping(true);
            p.prepare();
            p.start();
            player = p;
        } catch (Exception e) {
            restoreVolume();
            throw new IllegalStateException("alarm playback failed");
        }
        rx = new BroadcastReceiver() {
            @Override public void onReceive(Context c, Intent i) { stop(); onStopped.run(); }
        };
        IntentFilter f = new IntentFilter(ACTION_STOP);
        f.addAction(Intent.ACTION_USER_PRESENT);
        ctx.registerReceiver(rx, f, Context.RECEIVER_NOT_EXPORTED);
        notifyUser(nm, dnd);
        return dnd ? "alarm" : "alarm,no-dnd-access";
    }

    @Override public synchronized void stop() {
        if (player != null) {
            try { player.stop(); } catch (RuntimeException ignored) { /* already stopped */ }
            player.release();
            player = null;
        }
        if (rx != null) {
            try { ctx.unregisterReceiver(rx); } catch (RuntimeException ignored) { /* already gone */ }
            rx = null;
        }
        restoreVolume();
        ctx.getSystemService(NotificationManager.class).cancel(NOTIF_ID);
    }

    private void restoreVolume() {
        if (audio != null && savedVolume >= 0) {
            try { audio.setStreamVolume(AudioManager.STREAM_ALARM, savedVolume, 0); } catch (RuntimeException ignored) { /* best effort */ }
        }
        savedVolume = -1;
    }

    private void notifyUser(NotificationManager nm, boolean dnd) {
        NotificationChannel ch = new NotificationChannel(CHANNEL, "Find my phone", NotificationManager.IMPORTANCE_HIGH);
        if (dnd) ch.setBypassDnd(true);
        ch.setSound(null, null); // the alarm is played by the MediaPlayer, not the channel
        nm.createNotificationChannel(ch);
        Intent open = ctx.getPackageManager().getLaunchIntentForPackage(ctx.getPackageName());
        int flags = PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT;
        PendingIntent show = PendingIntent.getActivity(ctx, 0, open, flags);
        PendingIntent stop = PendingIntent.getBroadcast(ctx, 0, new Intent(ACTION_STOP).setPackage(ctx.getPackageName()), flags);
        Notification n = new Notification.Builder(ctx, CHANNEL)
                .setContentTitle("Your computer is ringing this phone")
                .setSmallIcon(android.R.drawable.ic_lock_idle_alarm)
                .setCategory(Notification.CATEGORY_ALARM)
                .setOngoing(true)
                .setContentIntent(show)
                .setFullScreenIntent(show, true)
                .addAction(new Notification.Action.Builder(null, "Stop", stop).build())
                .build();
        nm.notify(NOTIF_ID, n);
    }
}

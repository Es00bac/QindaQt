package org.qindaqt.link.engine;

import java.io.IOException;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import org.qindaqt.link.proto.Json;

/**
 * The engine's {@link LinkEngine.Connector}: walks {@link EndpointPlanner}'s order and returns the first
 * address that completes the TLS handshake. Owns discovery's lifetime (on only while no session is up)
 * and the UI-facing facts about LAN connection. Android-free; the {@link Opener} supplies TLS.
 *
 * AGENT-GUARD: the engine thread calls connect()/transport()/upgradeAvailable(); the UI thread calls
 * selectDesktop()/setManualAddress()/uiJson(). Shared fields are volatile or synchronized. A handshake
 * success is only transport reachability: the Session still enforces the pin (ADR-0434).
 */
public final class EndpointConnector implements LinkEngine.Connector {
    /** Opens a TLS link to one endpoint or throws; blocking, bounded by its own timeouts. */
    public interface Opener {
        Link open(Endpoint endpoint) throws IOException;
    }

    private final Opener opener;
    private final PinStore pins;
    private final LanMemory memory;
    private final DesktopDiscovery discovery;
    private final DiscoveryTable table = new DiscoveryTable();
    private volatile String selectedId;
    private volatile Endpoint active;
    private volatile String manualError = "";
    private Link pendingUsb; // guarded by this

    /** @param discovery may be null (no mDNS, for example in tests or when disabled) */
    public EndpointConnector(Opener opener, PinStore pins, LanMemory memory, DesktopDiscovery discovery) {
        this.opener = opener;
        this.pins = pins;
        this.memory = memory;
        this.discovery = discovery;
    }

    @Override public Link connect() throws IOException {
        startDiscovery();
        Link pre = takePendingUsb();
        if (pre != null) { connected(Endpoint.usb()); return pre; }
        IOException last = null;
        for (Endpoint e : currentPlan()) {
            try {
                Link l = opener.open(e);
                if (!matchesPin(l)) {
                    // AGENT-GUARD: a host that answers with the wrong key must not shadow the real endpoint;
                    // the Session would stop on it ("different desktop") and the engine would retry it first.
                    l.close();
                    last = new IOException("endpoint presented a different key");
                    continue;
                }
                connected(e);
                return l;
            } catch (IOException ex) {
                last = ex;
            }
        }
        throw last != null ? last : new IOException("no endpoint to try");
    }

    @Override public String transport() {
        Endpoint a = active;
        return a == null ? "none" : a.transport();
    }

    /**
     * While on LAN, true when the USB listener answers a full handshake. The open link is kept for the
     * next connect(); the desktop already replaces the LAN session when it arrives (ADR-0434 item 6), so
     * the caller should end the current session.
     */
    @Override public boolean upgradeAvailable() {
        Endpoint a = active;
        if (a == null || a.kind == Endpoint.Kind.USB) return false;
        synchronized (this) { if (pendingUsb != null) return true; }
        try {
            Link l = opener.open(Endpoint.usb());
            if (!matchesPin(l)) { l.close(); return false; }
            synchronized (this) { pendingUsb = l; }
            return true;
        } catch (IOException e) {
            return false;
        }
    }

    @Override public void disconnected() { active = null; }

    /** Unpaired phones connect only to the desktop the user picked (or the manual address). Null clears. */
    public void selectDesktop(String id) { selectedId = id; }

    /** Saves the manual address; returns "" on success or the message to show. An empty string clears it. */
    public String setManualAddress(String raw) {
        try {
            memory.setManual(raw);
            manualError = "";
        } catch (IllegalArgumentException e) {
            manualError = e.getMessage();
        } catch (IOException e) {
            manualError = "Could not save the address";
        }
        return manualError;
    }

    /**
     * Facts for the Connection section: {manual, manualError, discovering, selected, desktops:[{id,name}]}.
     * AGENT-CONTRACT: parsed by LinkBridge.cpp and ConnectionSection.qml. Carries no discovered addresses.
     */
    public String uiJson() {
        Map<String, Object> m = new LinkedHashMap<>();
        m.put("manual", memory.manualText());
        m.put("manualError", manualError);
        m.put("discovering", table.active());
        m.put("selected", selectedId == null ? "" : selectedId);
        List<Object> list = new ArrayList<>();
        for (DiscoveredDesktop d : table.snapshot()) {
            Map<String, Object> o = new LinkedHashMap<>();
            o.put("id", d.id);
            o.put("name", d.name);
            list.add(o);
        }
        m.put("desktops", list);
        return Json.write(m);
    }

    /** True when unpaired (nothing to check yet) or the link's proven key is the pinned desktop's. */
    private boolean matchesPin(Link l) {
        String pinned = pins.deviceId();
        return pinned == null || pinned.equals(org.qindaqt.link.proto.Pairing.hex(l.peerSpki()));
    }

    private List<Endpoint> currentPlan() {
        return EndpointPlanner.plan(pins.deviceId(), selectedId, memory.lastLan(), table.snapshot(), memory.manual());
    }

    private void startDiscovery() {
        if (discovery == null || table.active()) return;
        table.setActive(true);
        discovery.start(table);
    }

    private void stopDiscovery() {
        if (discovery == null || !table.active()) return;
        discovery.stop();
        table.setActive(false);
        table.clear();
    }

    private void connected(Endpoint e) {
        active = e;
        stopDiscovery();
        if (e.kind != Endpoint.Kind.USB) {
            try { memory.setLastLan(e); } catch (IOException ignored) { /* remembering is best effort */ }
        }
    }

    private synchronized Link takePendingUsb() {
        Link l = pendingUsb;
        pendingUsb = null;
        return l;
    }
}

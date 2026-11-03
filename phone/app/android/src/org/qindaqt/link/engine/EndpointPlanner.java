package org.qindaqt.link.engine;

import java.util.ArrayList;
import java.util.List;

/**
 * Decides which addresses to try, in order (docs/wiki/architecture/qindalink.md, "Connection order"):
 * USB loopback, the last LAN address that worked, discovered desktops, then the manual address.
 * Pure function; Android-free.
 *
 * AGENT-GUARD: discovery is a hint, never authority. A paired phone only tries discovered desktops whose
 * TXT id is a prefix of the pinned SPKI id. An unpaired phone tries a discovered desktop only after the
 * user picked it; otherwise it would offer pairing to whichever desktop answered first.
 */
public final class EndpointPlanner {
    private EndpointPlanner() {}

    public static List<Endpoint> plan(String pinnedId, String selectedId, Endpoint lastLan,
                                      List<DiscoveredDesktop> discovered, Endpoint manual) {
        List<Endpoint> out = new ArrayList<>();
        add(out, Endpoint.usb());
        if (lastLan != null) add(out, new Endpoint(lastLan.host, lastLan.port, Endpoint.Kind.LAN_LAST));
        if (discovered != null) {
            for (DiscoveredDesktop d : discovered) {
                boolean wanted = pinnedId != null ? pinnedId.startsWith(d.id) : d.id.equals(selectedId);
                if (wanted) add(out, d.endpoint());
            }
        }
        if (manual != null) add(out, manual);
        return out;
    }

    private static void add(List<Endpoint> out, Endpoint e) {
        if (!out.contains(e)) out.add(e);
    }
}

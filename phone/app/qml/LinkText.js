.pragma library
// Wording for engine states, shared by the screens so the same state reads the same everywhere.
// AGENT-CONTRACT: phase names are EngineState.Phase lower-cased (phone/app/android/.../EngineState.java).

function connected(phase) {
    return phase === "connected" || phase === "pairing" || phase === "confirm"
        || phase === "ready" || phase === "stale"
}

function transportLabel(transport) {
    return transport === "usb" ? "USB" : transport === "wifi" ? "Wi-Fi" : "Offline"
}

function headline(phase, transport) {
    switch (phase) {
    case "ready": return "Connected over " + transportLabel(transport)
    case "confirm": return "Confirm the pairing code"
    case "pairing": return "Pairing in progress"
    case "connected": return "Desktop found over " + transportLabel(transport)
    case "stale": return "Pairing needs renewing"
    case "connecting": return "Looking for your desktop"
    case "error": return "Something went wrong"
    default: return "Desktop offline"
    }
}

// Tk.Badge / Tk.Notice variant for a phase.
function variant(phase) {
    switch (phase) {
    case "ready": return "success"
    case "stale": case "connecting": case "confirm": case "pairing": return "warning"
    case "error": return "danger"
    case "connected": return "info"
    default: return "default"
    }
}

function spaced(code) {
    const c = (code || "").replace(/\s+/g, "")
    return c.length === 6 ? c.slice(0, 3) + " " + c.slice(3) : c
}

function batteryText(level, charging) {
    if (level < 0) return "Not reported yet"
    return level + " %" + (charging ? ", charging" : "")
}

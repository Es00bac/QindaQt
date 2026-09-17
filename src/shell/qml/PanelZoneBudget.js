// SPDX-License-Identifier: GPL-3.0-or-later
.pragma library

// Pure zone-extent arithmetic for a panel's three disjoint zone viewports. It
// takes plain numbers and returns plain numbers; it never touches an Item, a
// profile, or a setting.
//
// AGENT-CONTRACT: the zones must stay disjoint and inside the panel's content
// box. Every allocation here is clamped to what is left, so the three budgets
// can never sum above `extent`. A zone squeezed under its own minimum scrolls
// or folds inside its own viewport; it never overlaps a neighbour. Callers
// pass `extent` already net of the content inset.

// Horizontal panels allocate in reading order of importance: start, then end,
// then center (ADR-0188).
//
// AGENT-NOTE: the previous rule was `balanced()` below for every orientation,
// which gave each pressed zone `min(desired, remaining / zonesLeft)`. With
// three pressed zones the start zone could never exceed a third of the panel.
// The start zone hosts the active application's menu bar, whose desire is set
// by the application and is genuinely unbounded — an editor with fourteen
// top-level menus needs all fourteen visible — and a third of a 1920 px panel
// folded most of them into "+N". Serving start first fixes that; reserving
// each yielding zone's declared minimum is what keeps the clock and the tray
// from being squeezed to nothing in exchange.
//
// `demands` and `minimums` are {start, center, end} of numbers. Returns the
// same shape.
function byReadingOrder(extent, demands, minimums) {
    // The start zone needs no reservation of its own: it is served first.
    const start = allocate(demands.start, extent - minimums.end - minimums.center)
    const end = allocate(demands.end, extent - start - minimums.center)
    const center = allocate(demands.center, extent - start - end)
    return { start: start, center: center, end: end }
}

// Vertical panels and the dock keep the max-min fair split: their zones have
// no reading order to prefer, and the dock's own tile-fitting logic already
// resolves pressure before this function sees it.
function balanced(extent, demands) {
    const order = ["start", "center", "end"]
        .filter(name => demands[name] > 0)
        .sort((a, b) => demands[a] - demands[b])
    const result = { start: 0, center: 0, end: 0 }
    let remaining = extent
    for (let i = 0; i < order.length; ++i) {
        const budget = Math.min(demands[order[i]], remaining / (order.length - i))
        result[order[i]] = budget
        remaining -= budget
    }
    return result
}

function allocate(desired, available) {
    return Math.max(0, Math.min(desired, available))
}

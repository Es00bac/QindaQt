// SPDX-License-Identifier: LGPL-3.0-or-later
.pragma library

// Pure arrangement geometry for the Display route; it never touches the model.
//
// AGENT-CONTRACT: These rules mirror display_topology validation: logical size
// is pixel size / scale, transposed for 90/270 rotations, and rounded half-up
// exactly like DisplayTopology::logicalSizeForMode.
//   - two enabled displays may share an edge but must never overlap
//     (positiveOverlap), and edge contact is what keeps the desktop connected
//     (touchesEdge) so a gap only warns;
//   - coordinates stay within Display::kCoordinateBound.
// Change those in lockstep with the topology module, never independently.

var COORDINATE_BOUND = 1000000
var NUDGE_STEP = 10
var NUDGE_STEP_LARGE = 100
var SIDES = ["left", "right", "above", "below"]

function transposesDimensions(transform) {
    return transform === "90" || transform === "270"
            || transform === "flipX90" || transform === "flipX270"
}

function modeFor(output) {
    if (!output || !output.modes) {
        return null
    }
    var modes = output.modes
    for (var i = 0; i < modes.length; ++i) {
        if (modes[i].id === output.modeId) {
            return modes[i]
        }
    }
    return null
}

function roundHalfUp(value) {
    return Math.floor(value + 0.5)
}

// Logical extent for one advertised mode at a candidate scale. "integral" is
// false when the division leaves a fractional logical pixel, the case the
// topology reports as NonIntegralLogicalExtent.
function logicalSizeForScale(output, scale) {
    var mode = modeFor(output)
    var numericScale = Number(scale)
    if (mode === null || !isFinite(numericScale) || numericScale <= 0
            || !(mode.pixelWidth > 0) || !(mode.pixelHeight > 0)) {
        return null
    }
    var transposed = transposesDimensions(output.transform)
    var pixelWidth = transposed ? mode.pixelHeight : mode.pixelWidth
    var pixelHeight = transposed ? mode.pixelWidth : mode.pixelHeight
    var width = pixelWidth / numericScale
    var height = pixelHeight / numericScale
    var tolerance = 1e-9
    return {
        width: roundHalfUp(width),
        height: roundHalfUp(height),
        integral: Math.abs(width - Math.round(width)) < tolerance
                  && Math.abs(height - Math.round(height)) < tolerance
    }
}

// Logical size of an output as drafted. Derived from mode/scale/transform so
// the canvas stays truthful while a rejected draft leaves the facade's
// logicalWidth/logicalHeight at their last accepted values.
function logicalSizeFor(output) {
    var derived = logicalSizeForScale(output, output ? output.scale : NaN)
    if (derived !== null) {
        return { width: derived.width, height: derived.height }
    }
    return {
        width: Math.max(0, Number(output && output.logicalWidth) || 0),
        height: Math.max(0, Number(output && output.logicalHeight) || 0)
    }
}

function rectFor(output) {
    var size = logicalSizeFor(output)
    return {
        stableId: String(output.stableId),
        x: Number(output.positionX) || 0,
        y: Number(output.positionY) || 0,
        width: size.width,
        height: size.height,
        primary: output.primary === true
    }
}

function enabledRects(outputs) {
    var rects = []
    if (!outputs) {
        return rects
    }
    for (var i = 0; i < outputs.length; ++i) {
        var output = outputs[i]
        if (!output || !output.enabled) {
            continue
        }
        var rect = rectFor(output)
        if (rect.width > 0 && rect.height > 0) {
            rects.push(rect)
        }
    }
    return rects
}

function findRect(rects, stableId) {
    for (var i = 0; i < rects.length; ++i) {
        if (rects[i].stableId === stableId) {
            return rects[i]
        }
    }
    return null
}

function boundingBox(rects) {
    if (!rects || rects.length === 0) {
        return null
    }
    var left = Infinity, top = Infinity, right = -Infinity, bottom = -Infinity
    for (var i = 0; i < rects.length; ++i) {
        var rect = rects[i]
        left = Math.min(left, rect.x)
        top = Math.min(top, rect.y)
        right = Math.max(right, rect.x + rect.width)
        bottom = Math.max(bottom, rect.y + rect.height)
    }
    return { x: left, y: top, width: right - left, height: bottom - top }
}

function overlaps(a, b) {
    return a.x < b.x + b.width && b.x < a.x + a.width
            && a.y < b.y + b.height && b.y < a.y + a.height
}

function spansVertically(a, b) {
    return a.y < b.y + b.height && b.y < a.y + a.height
}

function spansHorizontally(a, b) {
    return a.x < b.x + b.width && b.x < a.x + a.width
}

function touches(a, b) {
    var aRight = a.x + a.width, bRight = b.x + b.width
    var aBottom = a.y + a.height, bBottom = b.y + b.height
    return ((aRight === b.x || bRight === a.x) && spansVertically(a, b))
            || ((aBottom === b.y || bBottom === a.y) && spansHorizontally(a, b))
}

function overlapsAny(rect, others, excludedId) {
    for (var i = 0; i < others.length; ++i) {
        if (others[i].stableId === excludedId) {
            continue
        }
        if (overlaps(rect, others[i])) {
            return true
        }
    }
    return false
}

function clamp(value, low, high) {
    return Math.min(Math.max(value, low), high)
}

function clampCoordinate(value) {
    return clamp(Math.round(value), -COORDINATE_BOUND, COORDINATE_BOUND)
}

function nearestAlignment(value, options, threshold) {
    var best = value
    var bestDistance = threshold
    for (var i = 0; i < options.length; ++i) {
        var distance = Math.abs(options[i] - value)
        if (distance <= bestDistance) {
            best = options[i]
            bestDistance = distance
        }
    }
    return best
}

// Snap a display to the attached, non-overlapping position nearest to the
// desired origin. The result always shares an edge with another enabled
// display (so the pointer can cross), never overlaps, and uses integers.
// alignThreshold (logical pixels) additionally pulls the free axis onto a
// neighbouring edge; pass 0 for keyboard nudges so they can slide freely.
function snapPosition(moving, others, desired, alignThreshold) {
    var width = moving.width, height = moving.height
    var desiredX = clampCoordinate(desired.x)
    var desiredY = clampCoordinate(desired.y)
    var rivals = []
    var xEdges = [], yEdges = []
    for (var i = 0; i < others.length; ++i) {
        var other = others[i]
        if (other.stableId === moving.stableId) {
            continue
        }
        rivals.push(other)
        xEdges.push(other.x, other.x + other.width - width)
        yEdges.push(other.y, other.y + other.height - height)
    }
    if (rivals.length === 0) {
        // A lone display has no relative arrangement; retain the topology origin.
        return { x: moving.x, y: moving.y, attached: false }
    }
    var best = null
    var bestDistance = Infinity
    for (var r = 0; r < rivals.length; ++r) {
        var rival = rivals[r]
        var rivalRight = rival.x + rival.width
        var rivalBottom = rival.y + rival.height
        var sideY = clamp(nearestAlignment(desiredY, yEdges, alignThreshold),
                          rival.y - height + 1, rivalBottom - 1)
        var sideX = clamp(nearestAlignment(desiredX, xEdges, alignThreshold),
                          rival.x - width + 1, rivalRight - 1)
        var candidates = [
            { x: rivalRight, y: sideY },
            { x: rival.x - width, y: sideY },
            { x: sideX, y: rivalBottom },
            { x: sideX, y: rival.y - height }
        ]
        for (var c = 0; c < candidates.length; ++c) {
            var candidate = { x: clampCoordinate(candidates[c].x),
                              y: clampCoordinate(candidates[c].y) }
            var rect = { x: candidate.x, y: candidate.y, width: width, height: height }
            if (overlapsAny(rect, rivals, moving.stableId)) {
                continue
            }
            var dx = candidate.x - desiredX
            var dy = candidate.y - desiredY
            var distance = dx * dx + dy * dy
            if (distance < bestDistance) {
                best = candidate
                bestDistance = distance
            }
        }
    }
    if (best === null) {
        return { x: moving.x, y: moving.y, attached: false }
    }
    return { x: best.x, y: best.y, attached: true }
}

// Place beside the reference; snapPosition resolves third-display collisions.
function placeBeside(moving, reference, side, others) {
    var desired
    if (side === "left") {
        desired = { x: reference.x - moving.width, y: reference.y }
    } else if (side === "right") {
        desired = { x: reference.x + reference.width, y: reference.y }
    } else if (side === "above") {
        desired = { x: reference.x, y: reference.y - moving.height }
    } else {
        desired = { x: reference.x, y: reference.y + reference.height }
    }
    return snapPosition(moving, others, desired, 0)
}

function indexById(rects) {
    var map = {}
    for (var i = 0; i < rects.length; ++i) {
        map[rects[i].stableId] = rects[i]
    }
    return map
}

function directNeighbourDelta(previous, oldChanged, newChanged) {
    var delta = { dx: 0, dy: 0 }
    var oldRight = oldChanged.x + oldChanged.width
    var oldBottom = oldChanged.y + oldChanged.height
    var newRight = newChanged.x + newChanged.width
    var newBottom = newChanged.y + newChanged.height
    var attachedRight = previous.x === oldRight && spansVertically(previous, oldChanged)
    var attachedLeft = previous.x + previous.width === oldChanged.x
                       && spansVertically(previous, oldChanged)
    var attachedBelow = previous.y === oldBottom && spansHorizontally(previous, oldChanged)
    var attachedAbove = previous.y + previous.height === oldChanged.y
                        && spansHorizontally(previous, oldChanged)
    if (attachedRight) {
        delta.dx = newRight - previous.x
    }
    if (attachedBelow) {
        delta.dy = newBottom - previous.y
    }
    if ((attachedRight || attachedLeft) && previous.y !== oldChanged.y
            && previous.y + previous.height === oldBottom) {
        delta.dy = (newBottom - previous.height) - previous.y
    }
    if ((attachedBelow || attachedAbove) && previous.x !== oldChanged.x
            && previous.x + previous.width === oldRight) {
        delta.dx = (newRight - previous.width) - previous.x
    }
    if (delta.dx === 0 && delta.dy === 0 && overlaps(previous, newChanged)) {
        // A detached neighbour that growth now covers: push it past the new
        // edge on the side it already occupied.
        if (previous.x >= oldRight) {
            delta.dx = newRight - previous.x
        } else if (previous.y >= oldBottom) {
            delta.dy = newBottom - previous.y
        }
    }
    return delta
}

// After one display changes logical size in place (scale, mode, or rotation),
// keep the displays that touched it attached to its new edges instead of
// letting them overlap or drift apart. The primary display never moves as a
// side effect. Any other resized display keeps its origin unless it only had
// neighbours on its right (or below): then it keeps that shared edge instead,
// so the one display being edited moves rather than the rest of the desktop.
// Returns the position updates needed; displays that moved independently
// between the two snapshots are left alone.
function reattachAfterResize(previousRects, nextRects, changedId) {
    var previousById = indexById(previousRects)
    var oldChanged = previousById[changedId]
    var newChanged = findRect(nextRects, changedId)
    if (!oldChanged || !newChanged) {
        return []
    }
    var i
    var hasLeft = false, hasRight = false, hasAbove = false, hasBelow = false
    for (i = 0; i < previousRects.length; ++i) {
        var neighbour = previousRects[i]
        if (neighbour.stableId === changedId) {
            continue
        }
        if (spansVertically(neighbour, oldChanged)) {
            hasLeft = hasLeft || neighbour.x + neighbour.width === oldChanged.x
            hasRight = hasRight || neighbour.x === oldChanged.x + oldChanged.width
        }
        if (spansHorizontally(neighbour, oldChanged)) {
            hasAbove = hasAbove || neighbour.y + neighbour.height === oldChanged.y
            hasBelow = hasBelow || neighbour.y === oldChanged.y + oldChanged.height
        }
    }
    var anchorsEdge = !oldChanged.primary
    var selfDx = anchorsEdge && hasRight && !hasLeft ? oldChanged.width - newChanged.width : 0
    var selfDy = anchorsEdge && hasBelow && !hasAbove ? oldChanged.height - newChanged.height : 0
    var anchored = { stableId: changedId, x: newChanged.x + selfDx,
                     y: newChanged.y + selfDy, width: newChanged.width,
                     height: newChanged.height }
    var deltas = {}
    if (selfDx !== 0 || selfDy !== 0) {
        deltas[changedId] = { dx: selfDx, dy: selfDy }
    }
    for (i = 0; i < nextRects.length; ++i) {
        var rect = nextRects[i]
        var previous = previousById[rect.stableId]
        if (rect.stableId === changedId || !previous
                || previous.x !== rect.x || previous.y !== rect.y) {
            continue
        }
        var delta = directNeighbourDelta(previous, oldChanged, anchored)
        if (delta.dx !== 0 || delta.dy !== 0) {
            deltas[rect.stableId] = delta
        }
    }
    propagateAlongRows(nextRects, previousById, deltas, changedId)
    return resolveMoves(nextRects, deltas)
}

// Carry an attached neighbour's translation through its row or column.
function propagateAlongRows(nextRects, previousById, deltas, changedId) {
    var progressed = true
    var guard = nextRects.length + 1
    while (progressed && guard-- > 0) {
        progressed = false
        for (var i = 0; i < nextRects.length; ++i) {
            var candidate = nextRects[i]
            if (candidate.stableId === changedId || deltas[candidate.stableId]) {
                continue
            }
            for (var movedId in deltas) {
                if (movedId === changedId) {
                    continue
                }
                var moved = previousById[movedId]
                var movedDelta = deltas[movedId]
                var propagated = { dx: 0, dy: 0 }
                if (movedDelta.dx !== 0 && candidate.x === moved.x + moved.width
                        && spansVertically(candidate, moved)) {
                    propagated.dx = movedDelta.dx
                }
                if (movedDelta.dy !== 0 && candidate.y === moved.y + moved.height
                        && spansHorizontally(candidate, moved)) {
                    propagated.dy = movedDelta.dy
                }
                if (propagated.dx !== 0 || propagated.dy !== 0) {
                    deltas[candidate.stableId] = propagated
                    progressed = true
                    break
                }
            }
        }
    }
}

// Apply the deltas, then let any display that still collides fall back to
// the nearest attached free position. Only real position changes are returned.
function resolveMoves(nextRects, deltas) {
    var placed = []
    var i
    for (i = 0; i < nextRects.length; ++i) {
        var source = nextRects[i]
        var shift = deltas[source.stableId] || { dx: 0, dy: 0 }
        placed.push({ stableId: source.stableId, x: source.x + shift.dx,
                      y: source.y + shift.dy, width: source.width,
                      height: source.height })
    }
    var moves = []
    for (i = 0; i < placed.length; ++i) {
        var target = placed[i]
        if (!deltas[target.stableId]) {
            continue
        }
        if (overlapsAny(target, placed, target.stableId)) {
            var resolved = snapPosition(target, placed, { x: target.x, y: target.y }, 0)
            target.x = resolved.x
            target.y = resolved.y
        }
        var current = findRect(nextRects, target.stableId)
        if (target.x !== current.x || target.y !== current.y) {
            moves.push({ stableId: target.stableId,
                         x: clampCoordinate(target.x),
                         y: clampCoordinate(target.y) })
        }
    }
    return moves
}

// Find the single enabled display whose logical size changed while its origin
// stayed put; that is the signature of a scale, mode, or rotation edit.
function resizedInPlace(previousRects, nextRects) {
    var previousById = indexById(previousRects)
    var found = null
    for (var i = 0; i < nextRects.length; ++i) {
        var rect = nextRects[i]
        var previous = previousById[rect.stableId]
        if (!previous || previous.x !== rect.x || previous.y !== rect.y) {
            continue
        }
        if (previous.width !== rect.width || previous.height !== rect.height) {
            if (found !== null) {
                return null
            }
            found = rect.stableId
        }
    }
    return found
}

function fitLayout(rects, width, height, padding, maxFit) {
    var box = boundingBox(rects)
    if (box === null) {
        return null
    }
    var availableWidth = Math.max(1, width - padding * 2)
    var availableHeight = Math.max(1, height - padding * 2)
    var fit = Math.min(availableWidth / Math.max(1, box.width),
                       availableHeight / Math.max(1, box.height), maxFit)
    return {
        fit: fit,
        originX: box.x,
        originY: box.y,
        offsetX: padding + (availableWidth - box.width * fit) / 2,
        offsetY: padding + (availableHeight - box.height * fit) / 2
    }
}

function toCanvas(layout, x, y) {
    return {
        x: (x - layout.originX) * layout.fit + layout.offsetX,
        y: (y - layout.originY) * layout.fit + layout.offsetY
    }
}

function typicalScaleFor(mode) {
    if (mode === null || !(mode.pixelWidth > 0)) {
        return 1.0
    }
    // Without a physical size the facade cannot give DPI; resolution alone
    // still separates the common cases (4K and above want 200%).
    if (mode.pixelWidth >= 3200) {
        return 2.0
    }
    if (mode.pixelWidth >= 2500) {
        return 1.25
    }
    return 1.0
}

function formatPercent(scale) {
    return Math.round(Number(scale) * 100) + "%"
}

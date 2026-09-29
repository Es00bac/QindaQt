// SPDX-License-Identifier: GPL-3.0-or-later
.pragma library

// Rectangle math for the tablet area canvases (ADR-0285). Every rectangle is
// {x, y, width, height} normalized to its surface (the unit square); an
// `aspect` is a normalized width / height ratio, and 0 leaves the shape free.
// The model re-validates whatever the canvas commits (normalizedArea in C++),
// so this file only has to keep the preview honest.

function clamp(value, low, high) {
    return Math.max(low, Math.min(high, value))
}

function same(first, second) {
    const close = (a, b) => Math.abs(a - b) < 1e-9
    return close(first.x, second.x) && close(first.y, second.y)
        && close(first.width, second.width) && close(first.height, second.height)
}

function contains(rect, point) {
    return point.x >= rect.x && point.x <= rect.x + rect.width
        && point.y >= rect.y && point.y <= rect.y + rect.height
}

// The corner OPPOSITE the one within reach of `point` (the anchor a corner
// drag resizes from), or null when no corner is within reach. Reach is given
// per axis because the canvas is rarely square.
function anchorOppositeCornerNear(rect, point, reachX, reachY) {
    const left = rect.x
    const right = rect.x + rect.width
    const top = rect.y
    const bottom = rect.y + rect.height
    const corners = [
        { x: left, y: top, anchor: { x: right, y: bottom } },
        { x: right, y: top, anchor: { x: left, y: bottom } },
        { x: left, y: bottom, anchor: { x: right, y: top } },
        { x: right, y: bottom, anchor: { x: left, y: top } }
    ]
    let best = null
    let bestDistance = 2
    for (let index = 0; index < corners.length; ++index) {
        const dx = Math.abs(point.x - corners[index].x) / Math.max(reachX, 1e-9)
        const dy = Math.abs(point.y - corners[index].y) / Math.max(reachY, 1e-9)
        const distance = Math.max(dx, dy)
        if (distance <= 1 && distance < bestDistance) {
            best = corners[index].anchor
            bestDistance = distance
        }
    }
    return best
}

// The same rectangle slid by (dx, dy), stopped at the surface's edges.
function moved(rect, dx, dy) {
    return {
        x: clamp(rect.x + dx, 0, 1 - rect.width),
        y: clamp(rect.y + dy, 0, 1 - rect.height),
        width: rect.width,
        height: rect.height
    }
}

// The rectangle spanned from a fixed `anchor` to `point`, inside the surface.
// With an aspect the drag's dominant direction decides the size and the
// other side follows; the result is scaled down, never moved, to fit.
function spanned(anchor, point, aspect) {
    const toRight = point.x >= anchor.x
    const toBottom = point.y >= anchor.y
    const roomX = toRight ? 1 - anchor.x : anchor.x
    const roomY = toBottom ? 1 - anchor.y : anchor.y
    let width = Math.min(Math.abs(point.x - anchor.x), roomX)
    let height = Math.min(Math.abs(point.y - anchor.y), roomY)
    if (aspect > 0) {
        if (width >= height * aspect)
            height = width / aspect
        else
            width = height * aspect
        const scale = Math.min(1,
                               width > 0 ? roomX / width : 1,
                               height > 0 ? roomY / height : 1)
        width *= scale
        height *= scale
    }
    return {
        x: toRight ? anchor.x : anchor.x - width,
        y: toBottom ? anchor.y : anchor.y - height,
        width: width,
        height: height
    }
}

// The rectangle grown or shrunk from its top-left corner. With an aspect the
// side that was not asked for follows. Returns `rect` unchanged when the
// change would take a side below `minimum`.
function resizedBy(rect, dWidth, dHeight, aspect, minimum) {
    let width = rect.width + dWidth
    let height = rect.height + dHeight
    if (aspect > 0) {
        if (dWidth !== 0) {
            height = width / aspect
        } else {
            width = height * aspect
        }
        const scale = Math.min(1, (1 - rect.x) / width, (1 - rect.y) / height)
        width *= scale
        height *= scale
    } else {
        width = Math.min(width, 1 - rect.x)
        height = Math.min(height, 1 - rect.y)
    }
    if (!(width >= minimum) || !(height >= minimum))
        return rect
    return { x: rect.x, y: rect.y, width: width, height: height }
}

// As much of the surface as the aspect allows, centred.
function largest(aspect) {
    if (!(aspect > 0))
        return { x: 0, y: 0, width: 1, height: 1 }
    let width = Math.min(1, aspect)
    let height = width / aspect
    if (height > 1) {
        height = 1
        width = aspect
    }
    return { x: (1 - width) / 2, y: (1 - height) / 2, width: width, height: height }
}

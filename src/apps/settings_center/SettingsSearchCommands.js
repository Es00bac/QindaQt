// SPDX-License-Identifier: GPL-3.0-or-later
.pragma library

// Pure helpers behind SettingsCommandPalette.qml (ADR-0257): turn the
// navigation controller's route list into Tk.CommandPalette commands and
// order them for a query. No QML context, no side effects, so the palette
// file keeps only presentation and the activation policy.

// AGENT-GUARD: the same General, Personalization, Hardware order and
// title sort as SettingsSidebar.orderedRoutes, so an empty query reads like
// the sidebar. Change both together.
function categoryRank(category) {
    return category === "General" ? 0 : category === "Personalization" ? 1 : 2
}

// Ctrl+1..Ctrl+9 then Ctrl+0 for registration indices 0..9; nothing after
// (ADR-0128, SettingsRouteShortcuts.qml).
function digitShortcut(registrationIndex) {
    return registrationIndex >= 0 && registrationIndex < 10
        ? "Ctrl+" + ((registrationIndex + 1) % 10) : ""
}

function asStringList(value) {
    if (value === undefined || value === null)
        return []
    const list = []
    for (let i = 0; i < value.length; ++i)
        list.push(String(value[i]))
    return list
}

// `routes` is navigation.routes (registration order). `formats` carries the
// translated templates: `unavailable` ("%1 ... %2": title, reason) and
// `destination` ("%1 ... %2": route title, destination title).
// Each command has the palette's {id, label, shortcut, section, iconName,
// keywords} plus the fields the activation policy and ranking need.
function buildCommands(routes, formats) {
    if (!routes)
        return []
    const indexed = []
    for (let i = 0; i < routes.length; ++i)
        indexed.push({ route: routes[i], registrationIndex: i })
    indexed.sort((left, right) => {
        const byCategory = categoryRank(left.route.category) - categoryRank(right.route.category)
        return byCategory !== 0 ? byCategory
                                : String(left.route.title).localeCompare(String(right.route.title))
    })

    const commands = []
    for (let i = 0; i < indexed.length; ++i) {
        const route = indexed[i].route
        const available = route.available === true
        const keywordList = asStringList(route.keywords)
        commands.push({
            id: "route:" + route.id,
            // AGENT-GUARD: an unavailable route stays listed and says why in
            // its own row, like the sidebar's unavailable tab; the reason is
            // text, never colour alone.
            label: available ? String(route.title)
                             : formats.unavailable.arg(route.title).arg(route.unavailableReason),
            shortcut: digitShortcut(indexed[i].registrationIndex),
            section: String(route.category),
            iconName: "",
            keywords: keywordList.join(", "),
            title: String(route.title),
            keywordList: keywordList,
            routeId: String(route.id),
            destination: "",
            available: available
        })
        // A destination opens a page, so an unavailable route offers none.
        const destinations = available && route.destinations ? route.destinations : []
        for (let d = 0; d < destinations.length; ++d) {
            const destination = destinations[d]
            const destinationKeywords = asStringList(destination.keywords)
            commands.push({
                id: "destination:" + route.id + "/" + destination.id,
                label: formats.destination.arg(route.title).arg(destination.title),
                shortcut: "",
                section: String(route.category),
                iconName: "",
                keywords: destinationKeywords.join(", "),
                title: String(destination.title),
                keywordList: destinationKeywords,
                routeId: String(route.id),
                destination: String(destination.id),
                available: true
            })
        }
    }
    return commands
}

// Lower is better. For a nonempty filter, paletteCommands keeps this complete
// relevance order in one section because Tk.CommandPalette groups by section.
// That makes Enter pick the obvious result ("battery" -> Power).
function matchRank(command, needle) {
    if (command.title.toLowerCase().startsWith(needle))
        return 0
    if (command.keywordList.some(keyword => keyword.toLowerCase() === needle))
        return 1
    if (command.label.toLowerCase().indexOf(needle) >= 0)
        return 2
    if (command.keywordList.some(keyword => keyword.toLowerCase().startsWith(needle)))
        return 3
    return 4
}

function orderedFor(commands, filterText) {
    const needle = String(filterText === undefined || filterText === null ? "" : filterText)
                       .trim().toLowerCase()
    if (needle.length === 0)
        return commands
    const ranked = commands.map((command, index) =>
                                ({ command: command, index: index, rank: matchRank(command, needle) }))
    ranked.sort((left, right) => left.rank - right.rank || left.index - right.index)
    return ranked.map(entry => entry.command)
}

// Tk.CommandPalette groups rows by `section`. Keep the sidebar sections for an
// empty filter, but put filtered matches together so grouping cannot scramble
// the relevance order across General, Personalization and Hardware.
function paletteCommands(commands, filterText, resultsSection) {
    const ordered = orderedFor(commands, filterText)
    const needle = String(filterText === undefined || filterText === null ? "" : filterText)
                       .trim()
    if (needle.length === 0)
        return ordered
    return ordered.map(command => Object.assign({}, command, { section: resultsSection }))
}

function find(commands, id) {
    for (let i = 0; i < commands.length; ++i) {
        if (commands[i].id === id)
            return commands[i]
    }
    return null
}

pragma Singleton
import QtQuick

// Single source of truth for all Librova icon paths.
//
// SVG path data is authored on the shared 24×24 icon grid.
// Render via LIcon { iconPath: LibrovaIcons.xxx }.
//
// Collection emoji are intentionally colourful — they represent user-chosen themes.
// Do not replace them with monochrome SVG.

QtObject {

    // ── Navigation ─────────────────────────────────────────────────────────────

    // Three books standing on a shelf
    readonly property string library:     "M3,21 L21,21 L21,23 L3,23 Z M4,5 L8,5 L8,21 L4,21 Z M10,7 L15,7 L15,21 L10,21 Z M17,9 L20,9 L20,21 L17,21 Z"

    // Bookmark silhouette — brand identity
    readonly property string book:        "M6,2 L18,2 L18,22 L12,17 L6,22 Z"

    // Arrow pointing into a tray (import)
    readonly property string import_:     "M12,16 L17,11 L14,11 L14,5 L10,5 L10,11 L7,11 Z M5,17 L19,17 L19,20 L5,20 Z"

    // Gear (even-odd: inner circle is transparent hole)
    readonly property string settings:    "M9.5,3 L14.5,3 L15.1,5.2 C15.57,5.38 16.02,5.62 16.43,5.91 L18.56,5.15 L21.05,9.46 L19.26,10.84 C19.3,11.22 19.3,11.6 19.26,11.98 L21.05,13.36 L18.56,17.67 L16.43,16.91 C16.02,17.2 15.57,17.44 15.1,17.62 L14.5,19.84 L9.5,19.84 L8.9,17.62 C8.43,17.44 7.98,17.2 7.57,16.91 L5.44,17.67 L2.95,13.36 L4.74,11.98 C4.7,11.6 4.7,11.22 4.74,10.84 L2.95,9.46 L5.44,5.15 L7.57,5.91 C7.98,5.62 8.43,5.38 8.9,5.2 Z M12,8.5 C10.07,8.5 8.5,10.07 8.5,12 C8.5,13.93 10.07,15.5 12,15.5 C13.93,15.5 15.5,13.93 15.5,12 C15.5,10.07 13.93,8.5 12,8.5 Z"

    // ── Library toolbar ─────────────────────────────────────────────────────────

    // Magnifying glass (even-odd: inner circle is the transparent lens)
    readonly property string search:      "M10.5,4 C14.09,4 17,6.91 17,10.5 C17,12.03 16.47,13.43 15.59,14.53 L20,18.94 L18.94,20 L14.53,15.59 C13.43,16.47 12.03,17 10.5,17 C6.91,17 4,14.09 4,10.5 C4,6.91 6.91,4 10.5,4 Z M10.5,6 C8.01,6 6,8.01 6,10.5 C6,12.99 8.01,15 10.5,15 C12.99,15 15,12.99 15,10.5 C15,8.01 12.99,6 10.5,6 Z"

    // Sort direction — open paths; Canvas closes them into filled triangles
    readonly property string sortAsc:     "M5,15 L12,9 L19,15"
    readonly property string sortDesc:    "M5,9 L12,15 L19,9"

    // Dropdown chevron — filled downward triangle
    // The source glyph grid is much larger than the shared icon viewport.
    readonly property string chevronDown: "M5,9 L12,15 L19,9"

    // Funnel
    readonly property string filter:      "M4,5 L20,5 L14,13 L14,19 L10,17 L10,13 Z"

    // ── Actions ─────────────────────────────────────────────────────────────────

    // Arrow up from tray (export)
    readonly property string export_:     "M12,4 L16,8 L13,8 L13,14 L11,14 L11,8 L8,8 Z M5,16 L19,16 L19,19 L5,19 Z"

    // Two overlapping sheets (even-odd: overlapping area is transparent)
    readonly property string copy:        "M9,5 C9,3.895 9.895,3 11,3 L18,3 C19.105,3 20,3.895 20,5 L20,14 C20,15.105 19.105,16 18,16 L11,16 C9.895,16 9,15.105 9,14 Z M6,8 C4.895,8 4,8.895 4,10 L4,19 C4,20.105 4.895,21 6,21 L13,21 C14.105,21 15,20.105 15,19 L15,18 L13,18 L13,19 L6,19 L6,10 L7,10 L7,8 Z"

    // Trash can (even-odd: lid slots become transparent)
    readonly property string trash:       "M9,4 L15,4 L16,6 L20,6 L20,8 L4,8 L4,6 L8,6 Z M6,9 L18,9 L17,20 L7,20 Z M9,11 L9,18 L11,18 L11,11 Z M13,11 L13,18 L15,18 L15,11 Z"

    // × close / dismiss
    readonly property string close:       "M6.4,5 L12,10.6 L17.6,5 L19,6.4 L13.4,12 L19,17.6 L17.6,19 L12,13.4 L6.4,19 L5,17.6 L10.6,12 L5,6.4 Z"

    // ── Import / folders ────────────────────────────────────────────────────────

    // Cloud with upward arrow — drop-zone and import area
    readonly property string uploadCloud: "M12,3 C15.87,3 19,5.92 19.46,9.68 C21.45,10.17 23,12.0 23,14.2 C23,16.85 20.85,19 18.2,19 L14,19 L14,13.41 L15.59,15 L17,13.59 L12,8.59 L7,13.59 L8.41,15 L10,13.41 L10,19 L5.8,19 C3.15,19 1,16.85 1,14.2 C1,12.0 2.55,10.17 4.54,9.68 C5,5.92 8.13,3 12,3 Z"

    // Open folder (even-odd: inner panel is transparent)
    readonly property string folderOpen:  "M3,6 C3,4.895 3.895,4 5,4 L9,4 L11,6 L19,6 C20.105,6 21,6.895 21,8 L21,17 C21,18.105 20.105,19 19,19 L5,19 C3.895,19 3,18.105 3,17 Z M6,9 L18,9 L16.4,16 L4.6,16 Z"

    // Folder + plus sign
    readonly property string addFolder:   "M3,6 C3,4.895 3.895,4 5,4 L9,4 L11,6 L19,6 C20.105,6 21,6.895 21,8 L21,17 C21,18.105 20.105,19 19,19 L5,19 C3.895,19 3,18.105 3,17 Z M12,9 L12,11.5 L14.5,11.5 L14.5,13.5 L12,13.5 L12,16 L10,16 L10,13.5 L7.5,13.5 L7.5,11.5 L10,11.5 L10,9 Z"

    // ── Status / misc ────────────────────────────────────────────────────────────

    // Info circle (even-odd: "i" body and dot are transparent cut-outs)
    readonly property string info:        "M12,3.5 C16.69,3.5 20.5,7.31 20.5,12 C20.5,16.69 16.69,20.5 12,20.5 C7.31,20.5 3.5,16.69 3.5,12 C3.5,7.31 7.31,3.5 12,3.5 Z M11,10 L11,17 L13,17 L13,10 Z M11,7 L11,9 L13,9 L13,7 Z"

    // Warning triangle (even-odd: exclamation mark is transparent)
    readonly property string warning:     "M12,2 L22,20 L2,20 Z M11,9 L11,14 L13,14 L13,9 Z M11,16 L11,18 L13,18 L13,16 Z"

    // Check circle (even-odd: checkmark notch is transparent)
    readonly property string check:       "M12,3.5 C16.69,3.5 20.5,7.31 20.5,12 C20.5,16.69 16.69,20.5 12,20.5 C7.31,20.5 3.5,16.69 3.5,12 C3.5,7.31 7.31,3.5 12,3.5 Z M7.5,12 L10.5,15 L16.5,9"

    // Circular refresh arrow
    readonly property string refresh:     "M17.65,6.35 C16.2,4.9 14.21,4 12,4 C7.58,4 4.01,7.58 4.01,12 C4.01,16.42 7.58,20 12,20 C15.73,20 18.84,17.45 19.73,14 L17.65,14 C16.83,16.33 14.61,18 12,18 C8.69,18 6,15.31 6,12 C6,8.69 8.69,6 12,6 C13.66,6 15.14,6.69 16.22,7.78 L13,11 L20,11 L20,4 Z"

    // Chevron pointing right
    readonly property string chevronRight:"M8,5 L15,12 L8,19 L6.6,17.6 L12.2,12 L6.6,6.4 Z"

    // ── Collections ─────────────────────────────────────────────────────────────

    // Collection icon options — intentionally colourful emoji.
    // Single source of truth shared by CollectionCreateDialog and SidebarNav.
    readonly property var collectionOptions: [
        { key: "books",      glyph: "📚" },
        { key: "collection", glyph: "🗂"  },
        { key: "archive",    glyph: "📦" },
        { key: "folder",     glyph: "📁" },
        { key: "bookmark",   glyph: "🔖" },
        { key: "star",       glyph: "⭐" },
        { key: "heart",      glyph: "❤️"  },
        { key: "compass",    glyph: "🧭" },
        { key: "map",        glyph: "🗺"  },
        { key: "clock",      glyph: "⏰" },
        { key: "sparkles",   glyph: "✨" },
        { key: "moon",       glyph: "🌙" },
        { key: "sun",        glyph: "☀️"  },
        { key: "leaf",       glyph: "🍃" },
        { key: "fire",       glyph: "🔥" },
        { key: "crown",      glyph: "👑" },
        { key: "pencil",     glyph: "✏️"  },
        { key: "masks",      glyph: "🎭" },
        { key: "castle",     glyph: "🏰" },
        { key: "ring",       glyph: "💍" }
    ]

    function collectionGlyph(iconKey) {
        for (var i = 0; i < collectionOptions.length; ++i) {
            if (collectionOptions[i].key === iconKey)
                return collectionOptions[i].glyph
        }
        return collectionOptions[0].glyph
    }
}

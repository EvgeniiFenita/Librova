pragma Singleton
import QtQuick

// All design tokens for the Librova warm-sepia palette.
// Import LibrovaQt in any QML file to access: LibrovaTheme.accent, etc.
QtObject {

    // ── Background layers (darkest → lightest) ───────────────────────────────
    readonly property color background:      "#0D0A07"
    readonly property color sidebar:         "#100C08"
    readonly property color surface:         "#161108"
    readonly property color surfaceMuted:    "#1C160C"
    readonly property color surfaceAlt:      "#221A0E"
    readonly property color surfaceHover:    "#2A2012"
    readonly property color surfaceElevated: "#2E2414"

    // ── Borders ───────────────────────────────────────────────────────────────
    readonly property color border:          "#2A200E"
    readonly property color borderStrong:    "#3A2E18"
    readonly property color sidebarBorder:   "#211A0C"

    // ── Accent (amber / gold) ─────────────────────────────────────────────────
    readonly property color accent:              "#F5A623"
    readonly property color accentBright:        "#FFD07A"
    readonly property color accentDim:           "#B87A1A"
    readonly property color accentMuted:         "#2A1C06"
    readonly property color accentSurface:       "#1A1003"
    readonly property color accentSurfaceHover:  "#251605"
    readonly property color accentBorder:        "#3D2C0A"

    // ── Navigation active state ───────────────────────────────────────────────
    readonly property color navActive:      "#1E1606"
    readonly property color navActiveHover: "#261C08"

    // ── Text hierarchy ────────────────────────────────────────────────────────
    readonly property color textPrimary:   "#F5EDD8"
    readonly property color textSecondary: "#A89880"
    readonly property color textMuted:     "#967E68"
    readonly property color textOnAccent:      "#0A0700"

    // ── Semantic: Success ─────────────────────────────────────────────────────
    readonly property color successSurface: "#08221A"
    readonly property color successBorder:  "#124E37"
    readonly property color success:        "#34D39A"

    // ── Semantic: Warning ─────────────────────────────────────────────────────
    readonly property color warningSurface: "#221806"
    readonly property color warning:        "#F9BF4C"

    // ── Semantic: Danger ──────────────────────────────────────────────────────
    readonly property color dangerSurface:  "#200A0C"
    readonly property color dangerBorder:   "#551820"
    readonly property color danger:         "#F87171"
    readonly property color dangerText:     "#FFDDDD"
    readonly property color dangerHoverBg:  "#3D0E12"

    // ── Special ───────────────────────────────────────────────────────────────
    readonly property color matte:            "#0A0700"
    readonly property color coverPlaceholder: "#F5EDD8"
    readonly property color dialogOverlay:    "#80000000"

    // ── Corner radii (px) ─────────────────────────────────────────────────────
    readonly property int radiusSmall:  8
    readonly property int radiusMedium: 12
    readonly property int radiusLarge:  18
    readonly property int radiusXLarge: 24

    // ── Spacing — 4 px base grid ──────────────────────────────────────────────
    readonly property int sp1: 4
    readonly property int sp2: 8
    readonly property int sp3: 12
    readonly property int sp4: 16
    readonly property int sp5: 20
    readonly property int sp6: 24
    readonly property int sp8: 32
    readonly property int paddingCard:        22
    readonly property int paddingCardCompact: 16

    // ── Control dimensions ────────────────────────────────────────────────────
    readonly property int controlHeight:    42
    readonly property int iconActionSize:   40
    readonly property int iconActionSmSize: 36
    readonly property int sidebarWidth:     244

    // ── Transition durations (ms) ─────────────────────────────────────────────
    readonly property int animFast:   120
    readonly property int animMedium: 150
}

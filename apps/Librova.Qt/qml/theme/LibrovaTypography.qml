pragma Singleton
import QtQuick

// Typography scale for the Librova design system.
// All font-related constants — family, sizes, weights, letter-spacing.
QtObject {

    // ── Font families ─────────────────────────────────────────────────────────
    readonly property string fontFamily:         "Segoe UI"
    readonly property string fontFamilyFallback: "sans-serif"

    // ── Size scale (px) ───────────────────────────────────────────────────────
    // SectionEyebrow, MetaLabel
    readonly property int sizeXs:   11
    // GenreChip, small labels
    readonly property int sizeSm:   13
    // Body (default), form labels, nav
    readonly property int sizeBase: 14
    // SectionTitle
    readonly property int sizeMd:   16
    // ViewTitle
    readonly property int sizeLg:   22
    // DisplayTitle
    readonly property int sizeXl:   32

    // ── Font weights (Qt font weight integers) ────────────────────────────────
    readonly property int weightRegular:  Font.Normal   // 400
    readonly property int weightSemiBold: Font.DemiBold // 600
    readonly property int weightBold:     Font.Bold     // 700
    readonly property int weightLight:    Font.Light    // 300

    // ── Letter spacing (px) ───────────────────────────────────────────────────
    readonly property real spacingEyebrow: 1.4
    readonly property real spacingMeta:    0.6
    readonly property real spacingBrand:   4.0

    // ── Line heights (px) ─────────────────────────────────────────────────────
    readonly property int lineHeightBase: 22
}

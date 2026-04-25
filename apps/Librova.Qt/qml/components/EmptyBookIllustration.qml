import QtQuick

// Multi-layer open-book illustration in a 220×150 coordinate space.
Canvas {
    width:  220
    height: 150

    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)

        // ── 1. Warm ambient glow ────────────────────────────────────────────────
        // Background glow.
        ctx.beginPath()
        ctx.ellipse(20, 8, 180, 134)
        ctx.fillStyle = "rgba(245, 166, 35, 0.188)"   // #30F5A623
        ctx.fill()

        // ── 2. Left page cover (dark leather) ──────────────────────────────────
        ctx.fillStyle = "#2A1A06"
        ctx.fillRect(25, 26, 85, 98)   // M25,26 L110,26 L110,124 L25,124 Z

        // ── 3. Left page paper (parchment) ─────────────────────────────────────
        ctx.fillStyle = "#CDB882"
        ctx.fillRect(27, 28, 81, 94)   // M27,28 L108,28 L108,122 L27,122 Z

        // ── 4. Left page text lines ─────────────────────────────────────────────
        ctx.strokeStyle = "rgba(122, 92, 56, 0.5)"   // #7A5C38 @ 50%
        ctx.lineWidth = 1.5
        ;[[33,41,103,41],[33,51,103,51],[33,61,103,61],[33,71,103,71],
          [33,81,89,81],[33,91,103,91],[33,101,103,101],[33,111,91,111]]
        .forEach(function(l) {
            ctx.beginPath(); ctx.moveTo(l[0],l[1]); ctx.lineTo(l[2],l[3]); ctx.stroke()
        })

        // ── 5. Right page cover (dark leather) ─────────────────────────────────
        ctx.fillStyle = "#2A1A06"
        ctx.fillRect(110, 26, 85, 98)   // M110,26 L195,26 L195,124 L110,124 Z

        // ── 6. Right page paper (parchment) ────────────────────────────────────
        ctx.fillStyle = "#CDB882"
        ctx.fillRect(112, 28, 81, 94)   // M112,28 L193,28 L193,122 L112,122 Z

        // ── 7. Right page text lines ────────────────────────────────────────────
        ;[[117,41,187,41],[117,51,187,51],[117,61,187,61],[117,71,187,71],
          [117,81,175,81],[117,91,187,91],[117,101,187,101],[117,111,179,111]]
        .forEach(function(l) {
            ctx.beginPath(); ctx.moveTo(l[0],l[1]); ctx.lineTo(l[2],l[3]); ctx.stroke()
        })

        // ── 8. Page-turning leaf 1 (slight lift) ───────────────────────────────
        ctx.fillStyle   = "#C8B070"
        ctx.strokeStyle = "#A89050"
        ctx.lineWidth   = 0.75
        ctx.beginPath()
        ctx.moveTo(111, 26)
        ctx.bezierCurveTo(133, 22, 158, 23, 164, 30)
        ctx.bezierCurveTo(166, 50, 166, 100, 162, 116)
        ctx.bezierCurveTo(155, 123, 131, 123, 111, 124)
        ctx.closePath(); ctx.fill(); ctx.stroke()

        // ── 9. Page-turning leaf 2 (medium lift) ───────────────────────────────
        ctx.fillStyle   = "#D8C080"
        ctx.strokeStyle = "#B8A060"
        ctx.beginPath()
        ctx.moveTo(111, 26)
        ctx.bezierCurveTo(136, 16, 163, 17, 171, 25)
        ctx.bezierCurveTo(174, 46, 174, 104, 169, 119)
        ctx.bezierCurveTo(161, 129, 135, 129, 111, 124)
        ctx.closePath(); ctx.fill(); ctx.stroke()

        // ── 10. Page-turning leaf 3 (fully lifted) ─────────────────────────────
        ctx.fillStyle   = "#EAD090"
        ctx.strokeStyle = "#C8A864"
        ctx.beginPath()
        ctx.moveTo(111, 26)
        ctx.bezierCurveTo(140, 8, 168, 9, 178, 18)
        ctx.bezierCurveTo(182, 41, 182, 109, 176, 122)
        ctx.bezierCurveTo(167, 134, 139, 134, 111, 124)
        ctx.closePath(); ctx.fill(); ctx.stroke()

        // ── 11. Spine strip ─────────────────────────────────────────────────────
        ctx.fillStyle = "#1A1006"
        ctx.fillRect(106, 24, 8, 102)   // M106,24 L114,24 L114,126 L106,126 Z

        // ── 12. Spine centre highlight ──────────────────────────────────────────
        ctx.fillStyle = "#3A2810"
        ctx.fillRect(109, 24, 2, 102)   // M109,24 L111,24 L111,126 L109,126 Z
    }
}

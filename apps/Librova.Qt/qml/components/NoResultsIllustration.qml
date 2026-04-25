import QtQuick

// Multi-layer magnifier illustration in a 150×150 coordinate space.
Canvas {
    width:  150
    height: 150

    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)

        // ── 1. Warm amber glow ──────────────────────────────────────────────────
        // Main magnifier lens.
        ctx.beginPath()
        ctx.ellipse(5, 5, 110, 110)
        ctx.fillStyle = "rgba(245, 166, 35, 0.145)"   // #25F5A623
        ctx.fill()

        // ── 2. Outer lens rim (dark leather) ───────────────────────────────────
        ctx.beginPath()
        ctx.ellipse(8, 8, 104, 104)
        ctx.fillStyle = "#2A1A06"
        ctx.fill()

        // ── 3. Inner lens face (parchment) ─────────────────────────────────────
        ctx.beginPath()
        ctx.ellipse(16, 16, 88, 88)
        ctx.fillStyle = "#CDB882"
        ctx.fill()

        // ── 4. Faint page lines inside lens ────────────────────────────────────
        ctx.strokeStyle = "rgba(122, 92, 56, 0.35)"   // #7A5C38 @ 35%
        ctx.lineWidth = 1.5
        ctx.lineCap   = "butt"
        ;[[34,48,86,48],[28,57,92,57],[26,66,94,66],[28,75,92,75],[34,84,86,84]]
        .forEach(function(l) {
            ctx.beginPath(); ctx.moveTo(l[0],l[1]); ctx.lineTo(l[2],l[3]); ctx.stroke()
        })

        // ── 5. × mark ──────────────────────────────────────────────────────────
        ctx.strokeStyle = "#3A1A06"
        ctx.lineWidth   = 4.5
        ctx.lineCap     = "round"
        ctx.beginPath(); ctx.moveTo(36, 36); ctx.lineTo(84, 84); ctx.stroke()
        ctx.beginPath(); ctx.moveTo(84, 36); ctx.lineTo(36, 84); ctx.stroke()

        // ── 6. Handle body (dark leather) ──────────────────────────────────────
        ctx.strokeStyle = "#1A0E04"
        ctx.lineWidth   = 14
        ctx.beginPath(); ctx.moveTo(95, 95); ctx.lineTo(130, 130); ctx.stroke()

        // ── 7. Handle highlight (inner warmth) ─────────────────────────────────
        ctx.strokeStyle = "#3A2810"
        ctx.lineWidth   = 5
        ctx.beginPath(); ctx.moveTo(95, 95); ctx.lineTo(130, 130); ctx.stroke()

        // ── 8. Accent ring (thin amber outline) ────────────────────────────────
        ctx.beginPath()
        ctx.ellipse(8, 8, 104, 104)
        ctx.strokeStyle = "rgba(245, 166, 35, 0.333)"   // #55F5A623
        ctx.lineWidth   = 1.5
        ctx.stroke()
    }
}

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent


class CIconGenerator:
    SIZES = (16, 24, 32, 48, 64, 128, 256)

    # Colour palette — matches app's warm sepia/amber design tokens
    ACCENT        = (245, 166,  35, 255)   # #F5A623 amber
    ACCENT_HIGH   = (255, 208, 122,  70)   # semi-transparent top highlight
    ACCENT_SPINE  = (184, 122,  26, 175)   # slightly darker left-edge strip
    SHADOW_COLOR  = (10,   7,   0,  95)    # soft bookmark drop-shadow
    BG_TOP        = ( 26,  16,   3, 255)   # #1A1003 warm dark
    BG_BOTTOM     = ( 13,  10,   7, 255)   # #0D0A07 near-black sepia

    @classmethod
    def Generate(cls, outputPath: str) -> None:
        output_path = Path(outputPath).resolve()
        output_path.parent.mkdir(parents=True, exist_ok=True)
        largest_icon = cls._CreateIconBitmap(256)
        largest_icon.save(output_path, format="ICO", sizes=[(s, s) for s in cls.SIZES])
        print(f"Generated icon: {output_path}")

    @classmethod
    def _CreateIconBitmap(cls, size: int) -> Image.Image:
        image = Image.new("RGBA", (size, size), (0, 0, 0, 0))

        # 1 — Rounded navy background with vertical gradient
        radius = max(2, int(size * 0.16))
        cls._DrawRoundedGradient(image, radius, cls.BG_TOP, cls.BG_BOTTOM)

        # 2 — Soft cyan glow centred behind the bookmark
        glow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        gs = int(size * 0.56)
        gx = (size - gs) // 2
        gy = int(size * 0.08)
        ImageDraw.Draw(glow).ellipse((gx, gy, gx + gs, gy + gs), fill=(245, 166, 35, 30))
        glow = glow.filter(ImageFilter.GaussianBlur(size * 0.09))
        image = Image.alpha_composite(image, glow)

        draw = ImageDraw.Draw(image)

        # 3 — Bookmark polygon (mirrors the PathIcon shape used in the sidebar)
        #   viewport: 24×24 units → mapped to [left=0.195, right=0.805, top=0.085, bot=0.915]
        #   notch centre at y ≈ 17/22 of the vertical span (same as path M6,2…L12,17…Z)
        bk_l = size * 0.195
        bk_r = size * 0.805
        bk_t = size * 0.085
        bk_b = size * 0.915
        h    = bk_b - bk_t
        notch_y = bk_t + h * 0.755

        pts = [
            (int(bk_l),       int(bk_t)),
            (int(bk_r),       int(bk_t)),
            (int(bk_r),       int(bk_b)),
            (int(size * 0.5), int(notch_y)),
            (int(bk_l),       int(bk_b)),
        ]

        # Drop shadow (offset by ~2%)
        so = max(1, int(size * 0.022))
        draw.polygon([(x + so, y + so) for x, y in pts], fill=cls.SHADOW_COLOR)

        # Main bookmark fill
        draw.polygon(pts, fill=cls.ACCENT)

        # Highlight band — top ~16% of bookmark height, semi-transparent
        ht = int(h * 0.16)
        draw.polygon(
            [(int(bk_l), int(bk_t)), (int(bk_r), int(bk_t)),
             (int(bk_r), int(bk_t) + ht), (int(bk_l), int(bk_t) + ht)],
            fill=cls.ACCENT_HIGH,
        )

        # Spine strip — left 10% of bookmark width, slightly darker
        sw = int((bk_r - bk_l) * 0.10)
        draw.polygon(
            [(int(bk_l),      int(bk_t)), (int(bk_l) + sw, int(bk_t)),
             (int(bk_l) + sw, int(bk_b)), (int(bk_l),      int(bk_b))],
            fill=cls.ACCENT_SPINE,
        )

        return image

    @staticmethod
    def _DrawRoundedGradient(
        image: Image.Image,
        radius: int,
        top_color: tuple[int, int, int, int],
        bottom_color: tuple[int, int, int, int],
    ) -> None:
        size = image.width
        gradient = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(gradient)
        for row in range(size):
            t = row / max(1, size - 1)
            color = tuple(
                int(top_color[c] + (bottom_color[c] - top_color[c]) * t)
                for c in range(4)
            )
            draw.line((0, row, size, row), fill=color)

        mask = Image.new("L", (size, size), 0)
        ImageDraw.Draw(mask).rounded_rectangle((0, 0, size - 1, size - 1), radius=radius, fill=255)
        image.paste(gradient, (0, 0), mask)


def ResolveOutputPath(raw_output_path: str) -> Path:
    output_path = Path(raw_output_path)
    if not output_path.is_absolute():
        output_path = REPO_ROOT / output_path
    return output_path.resolve()


def main() -> int:
    if len(sys.argv) > 1:
        output_path = str(ResolveOutputPath(sys.argv[1]))
    else:
        output_path = str(REPO_ROOT / "apps/Librova.UI/Assets/librova.ico")
    CIconGenerator.Generate(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

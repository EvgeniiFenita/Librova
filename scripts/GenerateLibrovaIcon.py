from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image, ImageDraw


class CIconGenerator:
    SIZES = (16, 24, 32, 48, 64, 128, 256)

    @classmethod
    def Generate(cls, outputPath: str) -> None:
        output_path = Path(outputPath).resolve()
        output_path.parent.mkdir(parents=True, exist_ok=True)

        largest_icon = cls._CreateIconBitmap(256)
        largest_icon.save(output_path, format="ICO", sizes=[(size, size) for size in cls.SIZES])
        print(f"Generated icon: {output_path}")

    @classmethod
    def _CreateIconBitmap(cls, size: int) -> Image.Image:
        image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(image)

        tile_bounds = (0, 0, size - 1, size - 1)
        cls._DrawVerticalGradient(
            draw,
            tile_bounds,
            0,
            top_color=(18, 24, 34, 255),
            bottom_color=(9, 13, 20, 255),
        )

        border_width = max(1, int(size * 0.03))
        draw.rectangle(tile_bounds, outline=(44, 58, 77, 255), width=border_width)

        book_left = size * 0.24
        book_top = size * 0.17
        book_right = size * 0.76
        book_bottom = size * 0.83
        book_radius = max(2, int(size * 0.045))
        book_outline = (10, 15, 22, 255)
        accent_color = (76, 194, 255, 255)

        draw.rounded_rectangle(
            (book_left, book_top, book_right, book_bottom),
            radius=book_radius,
            fill=accent_color,
            outline=book_outline,
            width=max(1, int(size * 0.02)),
        )

        spine_width = max(2, int(size * 0.08))
        draw.rounded_rectangle(
            (book_left, book_top, book_left + spine_width, book_bottom),
            radius=book_radius,
            fill=(49, 156, 212, 255),
            outline=book_outline,
            width=max(1, int(size * 0.02)),
        )

        page_margin = size * 0.035
        page_color = (230, 244, 252, 255)
        draw.rounded_rectangle(
            (book_left + spine_width + page_margin, book_top + page_margin, book_right - page_margin, book_bottom - page_margin),
            radius=max(1, int(book_radius * 0.7)),
            fill=page_color,
        )

        page_edge_x = book_right - page_margin
        line_width = max(1, int(size * 0.012))
        for ratio in (0.16, 0.34, 0.52, 0.7):
            y = book_top + (book_bottom - book_top) * ratio
            draw.line(
                (page_edge_x - (size * 0.06), y, page_edge_x, y),
                fill=(181, 201, 214, 255),
                width=line_width,
            )

        l_color = (17, 31, 46, 255)
        l_stroke = max(2, int(size * 0.065))
        l_left = book_left + spine_width + (size * 0.12)
        l_top = book_top + (size * 0.16)
        l_bottom = book_bottom - (size * 0.17)
        l_right = book_right - (size * 0.17)
        draw.line((l_left, l_top, l_left, l_bottom), fill=l_color, width=l_stroke)
        draw.line((l_left, l_bottom, l_right, l_bottom), fill=l_color, width=l_stroke)

        return image

    @staticmethod
    def _DrawVerticalGradient(
        draw: ImageDraw.ImageDraw,
        bounds: tuple[float, float, float, float],
        radius: int,
        top_color: tuple[int, int, int, int],
        bottom_color: tuple[int, int, int, int],
    ) -> None:
        x0, y0, x1, y1 = bounds
        height = max(1, int(y1 - y0))
        width = max(1, int(x1 - x0))
        gradient = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        gradient_draw = ImageDraw.Draw(gradient)

        for row in range(height):
            ratio = row / max(1, height - 1)
            color = tuple(
                int(top_color[channel] + ((bottom_color[channel] - top_color[channel]) * ratio))
                for channel in range(4)
            )
            gradient_draw.line((0, row, width, row), fill=color)

        mask = Image.new("L", (width, height), 0)
        mask_draw = ImageDraw.Draw(mask)
        mask_draw.rounded_rectangle((0, 0, width - 1, height - 1), radius=radius, fill=255)
        draw.bitmap((x0, y0), gradient, fill=None)
        draw._image.paste(gradient, (int(x0), int(y0)), mask)  # type: ignore[attr-defined]

def main() -> int:
    output_path = sys.argv[1] if len(sys.argv) > 1 else "apps/Librova.UI/Assets/librova.ico"
    CIconGenerator.Generate(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

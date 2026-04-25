---
name: win-desktop-design
description: "Guide for visual design, layout, theming, typography, and component choice in Qt/QML Windows desktop apps. Use when the question is primarily about appearance, UI structure, or Windows desktop UX."
---

# Modern Windows Desktop App Design With Qt/QML

## Goal

Produce Windows-desktop-oriented Qt/QML guidance that stays practical, consistent, and aligned with Librova's implemented design system.

## When to Use

- use this skill for layout, component, theming, navigation, typography, iconography, and motion questions in Qt/QML apps
- in Librova itself, treat `docs/UiDesignSystem.md` as the canonical design source
- do **not** override repository-specific tokens or component rules without updating `docs/UiDesignSystem.md`
- when implementing design changes in QML files (editing `.qml` views or components), use `$qt-qml` together with this skill

## Librova Rules

- Build actual app screens, not landing pages.
- Keep shell navigation, toolbar controls, dialogs, popups, and empty states consistent with existing QML components.
- Use `LibrovaTheme`, `LibrovaTypography`, `LibrovaIcons`, and shared controls from `apps/Librova.Qt/qml/components/`.
- Keep domain/business logic out of QML; route backend work through Qt controllers/adapters.
- Prefer stable dimensions for toolbars, cards, grids, and icon buttons so text and state changes do not shift layout.
- Popup and dropdown surfaces use the elevated warm surface background plus amber accent border defined in the design system.

## Close-Out

After UI changes, run `scripts\Run-Tests.ps1` and update `docs/UiDesignSystem.md` or `docs/ReleaseChecklist.md` when the visible workflow or component rules changed.

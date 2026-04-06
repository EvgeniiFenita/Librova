# Librova: Регрессия — Ручные Сценарии

> Инструкция по запуску и полный реестр сценариев: [ManualUiTestScenarios.md](../ManualUiTestScenarios.md)

## 1. Relaunch State Persistence

1. В `Import` выбери валидный `Source File`.
2. Нормально закрой приложение.
3. Снова запусти `.\Run-Librova.ps1`.
Ожидаемое поведение:
- раздел `Import` восстанавливает предыдущий source path и последнее состояние import flow, если оно было сохранено.

## 2. Smoke Pass After Release Build

1. Выполни:
   `.\Run-Tests.ps1 -Preset x64-release -Configuration Release`
2. Выполни:
   `.\Run-Librova.ps1 -Preset x64-release -Configuration Release`
Ожидаемое поведение:
- release build завершается успешно.
- UI успешно запускается из release artifacts.
- host корректно резолвится из `Release` layout.

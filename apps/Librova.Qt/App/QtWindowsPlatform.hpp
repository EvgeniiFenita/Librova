#pragma once

#include <QObject>
#include <QString>
#include <QWindow>

namespace LibrovaQt {

// Windows-specific desktop integration.
// Provides DWM title bar styling, taskbar progress, and clipboard access.
//
// Usage:
//   1. Construct before engine.load().
//   2. Call setWindow() after engine.load() to bind the main window.
//   3. Register as QML context property "windowsPlatform".
class QtWindowsPlatform : public QObject
{
    Q_OBJECT

public:
    explicit QtWindowsPlatform(QObject* parent = nullptr);
    ~QtWindowsPlatform() override;

    void setWindow(QWindow* window);

    // Clipboard — callable from QML.
    Q_INVOKABLE static void copyToClipboard(const QString& text);

    // Taskbar progress — called from main.cpp when import progress changes.
    void updateTaskbarProgress(int percent, bool active);

private:
    struct NativeHandles;
    NativeHandles* m_native = nullptr;
};

} // namespace LibrovaQt

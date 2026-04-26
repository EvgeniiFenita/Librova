#include "App/QtWindowsPlatform.hpp"

#include <QClipboard>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QWindow>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dwmapi.h>
#include <ShObjIdl_core.h>

Q_LOGGING_CATEGORY(lcPlatform, "librova.platform")

namespace LibrovaQt {

struct QtWindowsPlatform::NativeHandles
{
    HWND              hwnd     = nullptr;
    ITaskbarList3*    taskbar  = nullptr;

    ~NativeHandles()
    {
        if (taskbar)
        {
            taskbar->Release();
            taskbar = nullptr;
        }
    }
};

QtWindowsPlatform::QtWindowsPlatform(QObject* parent)
    : QObject(parent)
    , m_native(new NativeHandles())
{
}

QtWindowsPlatform::~QtWindowsPlatform()
{
    delete m_native;
}

void QtWindowsPlatform::setWindow(QWindow* window)
{
    if (!window)
        return;

    m_native->hwnd = reinterpret_cast<HWND>(window->winId());

    // Apply DWM dark title bar.
    const BOOL useDark = TRUE;
    const HRESULT hr = ::DwmSetWindowAttribute(
        m_native->hwnd,
        DWMWA_USE_IMMERSIVE_DARK_MODE,
        &useDark,
        sizeof(useDark));
    if (FAILED(hr))
        qCWarning(lcPlatform) << "DwmSetWindowAttribute(IMMERSIVE_DARK_MODE) failed:" << hr;
    else
        qCInfo(lcPlatform) << "DWM dark title bar applied";

    // Tint caption bar to match the app warm-sepia background (#0D0A07).
    // DWMWA_CAPTION_COLOR and DWMWA_TEXT_COLOR require Windows 11 (build 22000+);
    // on older systems the DWM calls silently fail — that is acceptable.
#ifndef DWMWA_CAPTION_COLOR
    constexpr DWORD DWMWA_CAPTION_COLOR = 35;
#endif
#ifndef DWMWA_TEXT_COLOR
    constexpr DWORD DWMWA_TEXT_COLOR = 36;
#endif
    // COLORREF is 0x00BBGGRR.
    const COLORREF captionColor = RGB(0x0D, 0x0A, 0x07); // #0D0A07 — app background
    const COLORREF textColor    = RGB(0xF5, 0xED, 0xD8); // #F5EDD8 — primary text

    if (FAILED(::DwmSetWindowAttribute(m_native->hwnd, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor))))
        qCWarning(lcPlatform) << "DwmSetWindowAttribute(CAPTION_COLOR) failed (Win10 expected)";
    else
        qCInfo(lcPlatform) << "DWM caption colour applied";

    if (FAILED(::DwmSetWindowAttribute(m_native->hwnd, DWMWA_TEXT_COLOR, &textColor, sizeof(textColor))))
        qCWarning(lcPlatform) << "DwmSetWindowAttribute(TEXT_COLOR) failed (Win10 expected)";
    else
        qCInfo(lcPlatform) << "DWM caption text colour applied";

    // Initialise ITaskbarList3 for import progress.
    ITaskbarList3* tbl = nullptr;
    const HRESULT htask = ::CoCreateInstance(
        CLSID_TaskbarList,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&tbl));
    if (SUCCEEDED(htask) && tbl)
    {
        tbl->HrInit();
        m_native->taskbar = tbl;
        qCInfo(lcPlatform) << "ITaskbarList3 initialised";
    }
    else
    {
        qCWarning(lcPlatform) << "CoCreateInstance(CLSID_TaskbarList) failed:" << htask;
    }
}

void QtWindowsPlatform::updateTaskbarProgress(int percent, bool active)
{
    if (!m_native->hwnd || !m_native->taskbar)
        return;

    if (!active)
    {
        m_native->taskbar->SetProgressState(m_native->hwnd, TBPF_NOPROGRESS);
        return;
    }

    m_native->taskbar->SetProgressState(m_native->hwnd, TBPF_NORMAL);
    m_native->taskbar->SetProgressValue(m_native->hwnd,
        static_cast<ULONGLONG>(std::max(0, std::min(percent, 100))),
        100ULL);
}

// static
void QtWindowsPlatform::copyToClipboard(const QString& text)
{
    if (auto* clipboard = QGuiApplication::clipboard())
        clipboard->setText(text);
}

} // namespace LibrovaQt

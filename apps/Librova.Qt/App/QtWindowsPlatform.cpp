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

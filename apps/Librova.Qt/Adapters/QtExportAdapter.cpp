#include "Adapters/QtExportAdapter.hpp"

#include <memory>

#include "Adapters/QtStringConversions.hpp"
#include "App/LibraryExportFacade.hpp"
#include "Domain/BookFormat.hpp"

// Win32 native Save dialog — avoids QML FileDialog SaveFile crash with non-ASCII filenames.
// No QtWidgets dependency needed.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

#include <QLoggingCategory>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcExport, "librova.export")

namespace LibrovaQt {

QtExportAdapter::QtExportAdapter(IBackendDispatcher* backend, QObject* parent)
    : QObject(parent)
    , m_backend(backend)
{
}

void QtExportAdapter::showExportDialog(
    qint64 bookId,
    const QString& suggestedName,
    const QString& forcedFormat)
{
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    const std::wstring initDir  = docs.toStdWString();
    const std::wstring initFile = suggestedName.toStdWString();

    wchar_t buf[32768] = {};
    wcsncpy_s(buf, initFile.c_str(), initFile.size());

    OPENFILENAMEW ofn = {};
    ofn.lStructSize     = sizeof(ofn);
    ofn.hwndOwner       = nullptr;
    ofn.lpstrFilter     = L"EPUB files\0*.epub\0FB2 files\0*.fb2\0All files\0*.*\0";
    ofn.lpstrFile       = buf;
    ofn.nMaxFile        = static_cast<DWORD>(std::size(buf));
    ofn.lpstrInitialDir = initDir.c_str();
    ofn.lpstrTitle      = L"Export book";
    ofn.Flags           = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    const std::wstring defExt =
        forcedFormat.compare(QStringLiteral("fb2"), Qt::CaseInsensitive) == 0 ? L"fb2" : L"epub";
    ofn.lpstrDefExt = defExt.c_str();

    if (!GetSaveFileNameW(&ofn))
    {
        const DWORD error = CommDlgExtendedError();
        if (error != 0)
        {
            qCWarning(lcExport) << "showExportDialog failed err=" << error;
        }
        return;
    }

    const QString path = QString::fromWCharArray(buf);
    const QString fmt = !forcedFormat.isEmpty()
                            ? forcedFormat
                            : path.endsWith(QStringLiteral(".epub"), Qt::CaseInsensitive)
                                  ? QStringLiteral("epub")
                              : path.endsWith(QStringLiteral(".fb2"), Qt::CaseInsensitive)
                                  ? QStringLiteral("fb2")
                                  : QString{};
    exportBook(bookId, path, fmt);
}

void QtExportAdapter::exportBook(qint64 bookId, const QString& destinationPath, const QString& format)
{
    if (!m_backend->isOpen())
    {
        emit exportFailed(bookId, QStringLiteral("Backend not open"));
        return;
    }

    if (m_isBusy)
    {
        qCWarning(lcExport) << "ExportBook rejected because another export is already running: bookId=" << bookId;
        emit exportFailed(bookId, QStringLiteral("Another export is already running"));
        return;
    }

    Librova::Application::SExportBookRequest req;
    req.BookId          = Librova::Domain::SBookId{static_cast<std::int64_t>(bookId)};
    req.DestinationPath = qtToPath(destinationPath);

    if (!format.isEmpty())
    {
        const auto parsed = Librova::Domain::TryParseBookFormat(qtToUtf8(format));
        if (parsed.has_value())
            req.ExportFormat = *parsed;
    }

    auto result = std::make_shared<std::optional<std::filesystem::path>>();
    const qint64 bid = bookId;
    setBusy(true);

    m_backend->dispatch(
        [result, req](auto& app) {
            *result = app.ExportBook(req);
        },
        [this, result, bid]() {
            setBusy(false);
            if (result->has_value())
            {
                const QString path = qtFromPath(**result);
                qCInfo(lcExport) << "ExportBook success: bookId=" << bid << "path=" << path;
                emit exported(bid, path);
            }
            else
            {
                qCWarning(lcExport) << "ExportBook failed: bookId=" << bid;
                emit exportFailed(bid, QStringLiteral("Export failed"));
            }
        },
        [this, bid](const QString& error) {
            setBusy(false);
            qCWarning(lcExport) << "ExportBook failed:" << error << "bookId=" << bid;
            emit exportFailed(bid, error);
        });
}

void QtExportAdapter::setBusy(bool value)
{
    if (m_isBusy == value)
    {
        return;
    }

    m_isBusy = value;
    emit isBusyChanged();
}

} // namespace LibrovaQt

#include "App/QtLogging.hpp"

#include <QtCore/QtMessageHandler>
#include <QDebug>

#include "Foundation/Logging.hpp"
#include "Foundation/UnicodeConversion.hpp"

namespace LibrovaQt {

namespace {

QtMessageHandler g_previousHandler = nullptr;
std::filesystem::path g_currentLogFilePath;

void LibrovaQtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        if (g_previousHandler)
        {
            g_previousHandler(type, context, msg);
        }
        return;
    }

    const std::string utf8 = msg.toUtf8().toStdString();

    switch (type)
    {
        case QtDebugMsg:
            Librova::Logging::Debug("[Qt] {}", utf8);
            break;
        case QtInfoMsg:
            Librova::Logging::Info("[Qt] {}", utf8);
            break;
        case QtWarningMsg:
            Librova::Logging::Warn("[Qt] {}", utf8);
            break;
        case QtCriticalMsg:
            Librova::Logging::Error("[Qt] {}", utf8);
            break;
        case QtFatalMsg:
            Librova::Logging::Critical("[Qt] {}", utf8);
            break;
    }
}

} // namespace

void QtLogging::Initialize(const std::filesystem::path& logFilePath)
{
    Librova::Logging::CLogging::InitializeHostLogger(logFilePath);
    g_previousHandler = qInstallMessageHandler(LibrovaQtMessageHandler);
    g_currentLogFilePath = logFilePath;
    Librova::Logging::Info("QtLogging: initialized.");
}

void QtLogging::Reinitialize(const std::filesystem::path& logFilePath)
{
    if (g_currentLogFilePath == logFilePath)
    {
        return;
    }

    Shutdown();
    Initialize(logFilePath);
}

std::filesystem::path QtLogging::CurrentLogFilePath()
{
    return g_currentLogFilePath;
}

void QtLogging::Shutdown() noexcept
{
    Librova::Logging::InfoIfInitialized("QtLogging: shutting down.");
    qInstallMessageHandler(g_previousHandler);
    g_previousHandler = nullptr;
    Librova::Logging::CLogging::Shutdown();
    g_currentLogFilePath.clear();
}

} // namespace LibrovaQt

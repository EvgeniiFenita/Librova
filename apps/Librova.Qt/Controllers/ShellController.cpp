#include "Controllers/ShellController.hpp"

#include <optional>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include "App/ILibraryApplication.hpp"
#include "App/QtApplicationBackend.hpp"
#include "App/QtLogging.hpp"
#include "App/QtRuntimePaths.hpp"
#include "App/QtRuntimeWorkspaceMaintenance.hpp"
#include "Controllers/PreferencesStore.hpp"
#include "Foundation/Logging.hpp"
#include "Foundation/UnicodeConversion.hpp"

#include "App/QtLaunchOptions.hpp"

namespace LibrovaQt {

ShellController::ShellController(
    PreferencesStore* preferences,
    QtApplicationBackend* backend,
    QObject* parent)
    : QObject(parent)
    , m_preferences(preferences)
    , m_backend(backend)
{
    const QString exeDir = QFileInfo(qApp->applicationFilePath()).absolutePath();
    m_uiStateFilePath = QtRuntimePaths::DefaultShellStateFilePath(exeDir);
    m_uiPreferencesFilePath = QtRuntimePaths::DefaultPreferencesFilePath(exeDir);
    m_uiLogFilePath = QtRuntimePaths::DefaultLogFilePath(exeDir);
    m_statusText = QStringLiteral("Starting Librova...");
    m_startupGuidanceText = QStringLiteral(
        "The app is preparing the in-process backend and loading the selected library.");

    QObject::connect(
        m_backend, &QtApplicationBackend::opened, this, &ShellController::onBackendOpened);
    QObject::connect(
        m_backend, &QtApplicationBackend::openFailed, this, &ShellController::onBackendOpenFailed);
    QObject::connect(
        m_preferences,
        &PreferencesStore::converterExePathChanged,
        this,
        &ShellController::onConverterPreferenceChanged);
}

void ShellController::start(const SQtLaunchOptions& options)
{
    setState(EShellState::Idle);

    if (options.firstRun)
    {
        Librova::Logging::InfoIfInitialized("ShellController: first-run flag set.");
        m_statusText = QStringLiteral("Set up your library");
        m_startupGuidanceText = QStringLiteral(
            "Choose whether to create a new Librova library or open an existing one before the first launch.");
        updateShellMetadata();
        setState(EShellState::FirstRun);
        return;
    }

    if (options.startupErrorRecovery)
    {
        Librova::Logging::InfoIfInitialized("ShellController: startup-error-recovery flag set.");
        m_statusText = QStringLiteral("Startup failed.");
        m_startupGuidanceText = QStringLiteral(
            "Choose a different managed library root and retry startup from here.");
        updateShellMetadata();
        setState(EShellState::Recovery);
        return;
    }

    // CLI library root override takes precedence over saved preferences.
    if (options.libraryRoot.has_value() && !options.libraryRoot->isEmpty())
    {
        Librova::Logging::InfoIfInitialized(
            "ShellController: using CLI library root override.");
        openLibrary(*options.libraryRoot, options.createLibraryRoot);
        return;
    }

    const QString exeDir = QFileInfo(qApp->applicationFilePath()).absolutePath();
    const QString preferredLibraryRoot = QtRuntimePaths::ResolvePreferredLibraryRoot(
        m_preferences->libraryRoot(),
        m_preferences->portableLibraryRoot(),
        exeDir);
    if (!preferredLibraryRoot.isEmpty() && preferredLibraryRoot != m_preferences->libraryRoot())
    {
        m_preferences->setLibraryRoot(preferredLibraryRoot);
    }

    if (!m_preferences->HasLibraryRoot())
    {
        Librova::Logging::InfoIfInitialized(
            "ShellController: no library configured, entering first-run.");
        m_statusText = QStringLiteral("Set up your library");
        m_startupGuidanceText = QStringLiteral(
            "Choose whether to create a new Librova library or open an existing one before the first launch.");
        updateShellMetadata();
        setState(EShellState::FirstRun);
        return;
    }

    const QString root = m_preferences->libraryRoot();
    if (!QDir(root).exists())
    {
        Librova::Logging::WarnIfInitialized(
            "ShellController: saved library root '{}' does not exist, entering recovery.",
            root.toUtf8().toStdString());
        m_statusText = QStringLiteral("Startup failed.");
        m_startupGuidanceText = QStringLiteral(
            "The configured library root is unavailable. Choose another managed library root and retry startup.");
        updateShellMetadata();
        setState(EShellState::Recovery);
        return;
    }

    openLibrary(root, /*createNew=*/false);
}

void ShellController::handleFirstRunComplete(
    const QString& libraryRootPath,
    bool createNew,
    const QString& converterExePath)
{
    if (!converterExePath.isEmpty())
    {
        m_preferences->setConverterExePath(converterExePath);
    }
    openLibrary(libraryRootPath, createNew);
}

void ShellController::handleRecoveryComplete(const QString& libraryRootPath, bool createNew)
{
    openLibrary(libraryRootPath, createNew);
}

void ShellController::handleLibrarySwitch(const QString& libraryRootPath)
{
    if (m_state == EShellState::Opening)
    {
        return; // Ignore while already opening.
    }
    openLibrary(libraryRootPath, /*createNew=*/false);
}

void ShellController::openLibrary(const QString& libraryRootPath, bool createNew)
{
    setState(EShellState::Opening);
    m_lastAttemptedLibraryRoot = QDir(libraryRootPath).absolutePath();
    m_currentLibraryRoot.clear();
    m_statusText = QStringLiteral("Opening library...");
    m_startupGuidanceText = QStringLiteral(
        "Librova is preparing runtime workspaces, validating the library root, and opening the backend.");

    const std::filesystem::path libraryRoot =
        Librova::Unicode::PathFromUtf8(m_lastAttemptedLibraryRoot.toUtf8().toStdString());

    const QString exeDir = QFileInfo(qApp->applicationFilePath()).absolutePath();
    const auto workspaceRoot = QtRuntimePaths::RuntimeWorkspaceRoot(libraryRoot, exeDir);

    // Clean up stale transient workspace state from the previous session.
    QtRuntimeWorkspaceMaintenance::PrepareForSession(workspaceRoot);
    const auto runtimeLogPath = QtRuntimePaths::UiRuntimeLogFilePathForLibrary(libraryRoot, exeDir);
    const auto previousLogPath = QtLogging::CurrentLogFilePath();
    QtLogging::Reinitialize(runtimeLogPath);
    if (!createNew && !previousLogPath.empty() && previousLogPath != runtimeLogPath)
    {
        QtRuntimePaths::SyncRuntimeLogFile(previousLogPath, QtRuntimePaths::UiLogFilePathForLibrary(libraryRoot));
    }

    Librova::Application::SLibraryApplicationConfig config;
    config.LibraryRoot = libraryRoot;
    config.RuntimeWorkspaceRoot = workspaceRoot;
    config.ConverterWorkingDirectory = QtRuntimePaths::ConverterWorkingDirectory(libraryRoot, exeDir);
    config.ManagedStorageStagingRoot =
        QtRuntimePaths::ManagedStorageStagingRoot(libraryRoot, exeDir);
    config.LibraryOpenMode = createNew
        ? Librova::Application::ELibraryOpenMode::CreateNew
        : Librova::Application::ELibraryOpenMode::Open;

    const QString converterExe = m_preferences->converterExePath();
    if (!converterExe.isEmpty())
    {
        config.BuiltInFb2CngExePath =
            Librova::Unicode::PathFromUtf8(converterExe.toUtf8().toStdString());
    }

    m_uiLogFilePath = QString::fromUtf8(QtRuntimePaths::UiLogFilePathForLibrary(libraryRoot).u8string().c_str());
    m_runtimeWorkspaceRootPath = QString::fromUtf8(workspaceRoot.u8string().c_str());
    m_converterWorkingDirectoryPath =
        QString::fromUtf8(config.ConverterWorkingDirectory->u8string().c_str());
    m_managedStorageStagingRootPath =
        QString::fromUtf8(config.ManagedStorageStagingRoot->u8string().c_str());
    updateShellMetadata();

    m_backend->openAsync(config);
}

void ShellController::onBackendOpened()
{
    Librova::Logging::InfoIfInitialized("ShellController: backend opened successfully.");
    m_currentLibraryRoot = m_lastAttemptedLibraryRoot;
    m_preferences->setLibraryRoot(m_currentLibraryRoot);
    const QString exeDir = QFileInfo(qApp->applicationFilePath()).absolutePath();
    m_preferences->setPortableLibraryRoot(
        QtRuntimePaths::BuildPortableLibraryRootPreference(m_currentLibraryRoot, exeDir));
    m_statusText = QStringLiteral("Library opened.");
    m_startupGuidanceText = QStringLiteral(
        "The library is ready. Browse books, import new titles, or adjust settings.");
    updateShellMetadata();
    setState(EShellState::Ready);
}

void ShellController::onBackendOpenFailed(const QString& error)
{
    Librova::Logging::WarnIfInitialized(
        "ShellController: backend open failed: {}", error.toUtf8().toStdString());

    // Distinguish: library path exists but failed to open → DamagedLibrary.
    const QString root = m_lastAttemptedLibraryRoot;
    if (!root.isEmpty() && QDir(root).exists())
    {
        m_statusText = QStringLiteral("Startup failed.");
        m_startupGuidanceText = QStringLiteral(
            "The configured library exists but could not be opened. Choose another managed library root or repair the library manually.");
        updateShellMetadata();
        setState(EShellState::DamagedLibrary);
    }
    else
    {
        setFatalError(error);
    }
}

void ShellController::onConverterPreferenceChanged()
{
    Q_EMIT hasConverterChanged();

    if (m_state != EShellState::Ready || !m_preferences->HasLibraryRoot())
    {
        return;
    }

    Librova::Logging::InfoIfInitialized(
        "ShellController: converter preferences changed, reloading library session.");
    openLibrary(m_preferences->libraryRoot(), /*createNew=*/false);
}

void ShellController::setState(EShellState state)
{
    if (m_state != state)
    {
        m_state = state;
        Q_EMIT currentStateChanged();
    }
}

void ShellController::setFatalError(const QString& message)
{
    m_fatalErrorMessage = message;
    Q_EMIT fatalErrorMessageChanged();
    setState(EShellState::FatalError);
}

EShellState ShellController::currentState() const { return m_state; }

QString ShellController::currentStateName() const
{
    switch (m_state)
    {
    case EShellState::Idle:           return QStringLiteral("idle");
    case EShellState::FirstRun:       return QStringLiteral("firstRun");
    case EShellState::Opening:        return QStringLiteral("opening");
    case EShellState::Ready:          return QStringLiteral("ready");
    case EShellState::DamagedLibrary: return QStringLiteral("damagedLibrary");
    case EShellState::Recovery:       return QStringLiteral("recovery");
    case EShellState::FatalError:     return QStringLiteral("fatalError");
    }
    return QStringLiteral("idle");
}

QString ShellController::fatalErrorMessage() const { return m_fatalErrorMessage; }

bool ShellController::hasConverter() const
{
    return !m_preferences->converterExePath().isEmpty();
}

QString ShellController::statusText() const { return m_statusText; }

QString ShellController::startupGuidanceText() const { return m_startupGuidanceText; }

QString ShellController::currentLibraryRoot() const
{
    return m_currentLibraryRoot.isEmpty() ? m_preferences->libraryRoot() : m_currentLibraryRoot;
}

QString ShellController::uiLogFilePath() const { return m_uiLogFilePath; }

QString ShellController::uiStateFilePath() const { return m_uiStateFilePath; }

QString ShellController::uiPreferencesFilePath() const { return m_uiPreferencesFilePath; }

QString ShellController::runtimeWorkspaceRootPath() const { return m_runtimeWorkspaceRootPath; }

QString ShellController::converterWorkingDirectoryPath() const
{
    return m_converterWorkingDirectoryPath;
}

QString ShellController::managedStorageStagingRootPath() const
{
    return m_managedStorageStagingRootPath;
}

void ShellController::updateShellMetadata()
{
    Q_EMIT shellMetadataChanged();
}

} // namespace LibrovaQt

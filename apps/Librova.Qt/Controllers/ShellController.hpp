#pragma once

#include <optional>

#include <QObject>
#include <QString>

namespace LibrovaQt {

class PreferencesStore;
class QtApplicationBackend;
struct SQtLaunchOptions;

enum class EShellState
{
    Idle,
    FirstRun,
    Opening,
    Ready,
    DamagedLibrary,
    Recovery,
    FatalError,
};

// Drives the application startup state machine.
// Composed in main.cpp with injected PreferencesStore and QtApplicationBackend.
class ShellController : public QObject
{
    Q_OBJECT
    Q_ENUM(EShellState)

    Q_PROPERTY(EShellState currentState READ currentState NOTIFY currentStateChanged)
    Q_PROPERTY(QString currentStateName READ currentStateName NOTIFY currentStateChanged)
    Q_PROPERTY(QString fatalErrorMessage READ fatalErrorMessage NOTIFY fatalErrorMessageChanged)
    Q_PROPERTY(bool hasConverter READ hasConverter NOTIFY hasConverterChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY shellMetadataChanged)
    Q_PROPERTY(QString startupGuidanceText READ startupGuidanceText NOTIFY shellMetadataChanged)
    Q_PROPERTY(QString currentLibraryRoot READ currentLibraryRoot NOTIFY shellMetadataChanged)
    Q_PROPERTY(QString uiLogFilePath READ uiLogFilePath NOTIFY shellMetadataChanged)
    Q_PROPERTY(QString uiStateFilePath READ uiStateFilePath NOTIFY shellMetadataChanged)
    Q_PROPERTY(QString uiPreferencesFilePath READ uiPreferencesFilePath NOTIFY shellMetadataChanged)
    Q_PROPERTY(QString runtimeWorkspaceRootPath READ runtimeWorkspaceRootPath NOTIFY shellMetadataChanged)
    Q_PROPERTY(QString converterWorkingDirectoryPath READ converterWorkingDirectoryPath NOTIFY shellMetadataChanged)
    Q_PROPERTY(QString managedStorageStagingRootPath READ managedStorageStagingRootPath NOTIFY shellMetadataChanged)

public:
    explicit ShellController(
        PreferencesStore* preferences,
        QtApplicationBackend* backend,
        QObject* parent = nullptr);

    // Begins the startup sequence. Call once after QML engine is loaded.
    void start(const SQtLaunchOptions& options);

    [[nodiscard]] EShellState currentState() const;
    [[nodiscard]] QString currentStateName() const;
    [[nodiscard]] QString fatalErrorMessage() const;
    [[nodiscard]] bool hasConverter() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString startupGuidanceText() const;
    [[nodiscard]] QString currentLibraryRoot() const;
    [[nodiscard]] QString uiLogFilePath() const;
    [[nodiscard]] QString uiStateFilePath() const;
    [[nodiscard]] QString uiPreferencesFilePath() const;
    [[nodiscard]] QString runtimeWorkspaceRootPath() const;
    [[nodiscard]] QString converterWorkingDirectoryPath() const;
    [[nodiscard]] QString managedStorageStagingRootPath() const;

    // QML-invokable actions driven by the user completing a setup step.
    Q_INVOKABLE void handleFirstRunComplete(
        const QString& libraryRootPath,
        bool createNew,
        const QString& converterExePath = {});
    Q_INVOKABLE void handleRecoveryComplete(const QString& libraryRootPath, bool createNew);
    Q_INVOKABLE void handleLibrarySwitch(const QString& libraryRootPath);

Q_SIGNALS:
    void currentStateChanged();
    void fatalErrorMessageChanged();
    void hasConverterChanged();
    void shellMetadataChanged();

private:
    void openLibrary(const QString& libraryRootPath, bool createNew = false);
    void setState(EShellState state);
    void setFatalError(const QString& message);
    void updateShellMetadata();

    void onBackendOpened();
    void onBackendOpenFailed(const QString& error);
    void onConverterPreferenceChanged();

    PreferencesStore* m_preferences;
    QtApplicationBackend* m_backend;

    EShellState m_state = EShellState::Idle;
    QString m_fatalErrorMessage;
    QString m_statusText;
    QString m_startupGuidanceText;
    QString m_currentLibraryRoot;
    QString m_uiStateFilePath;
    QString m_uiPreferencesFilePath;
    QString m_uiLogFilePath;
    QString m_runtimeWorkspaceRootPath;
    QString m_converterWorkingDirectoryPath;
    QString m_managedStorageStagingRootPath;
    QString m_lastAttemptedLibraryRoot;
};

} // namespace LibrovaQt

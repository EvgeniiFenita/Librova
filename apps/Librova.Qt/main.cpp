#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "Adapters/QtCatalogAdapter.hpp"
#include "Adapters/QtExportAdapter.hpp"
#include "Adapters/QtImportAdapter.hpp"
#include "Adapters/QtTrashAdapter.hpp"
#include "App/QtApplicationBackend.hpp"
#include "App/QtLaunchOptions.hpp"
#include "App/QtLogging.hpp"
#include "App/QtRuntimePaths.hpp"
#include "App/QtWindowsPlatform.hpp"
#include "Controllers/ConverterValidationController.hpp"
#include "Controllers/FirstRunController.hpp"
#include "Controllers/PreferencesStore.hpp"
#include "Controllers/ShellController.hpp"
#include "Controllers/ShellStateStore.hpp"
#include "Foundation/UnicodeConversion.hpp"
#include "Models/ImportJobListModel.hpp"

int main(int argc, char* argv[])
{
    // Must be set before QGuiApplication construction.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Librova"));
    app.setOrganizationName(QStringLiteral("Librova"));
    app.setApplicationVersion(QStringLiteral(LIBROVA_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/librova.ico")));

    const auto opts = LibrovaQt::SQtLaunchOptions::Parse(app.arguments());

    const QString exeDir = QFileInfo(app.applicationFilePath()).absolutePath();

    const QString logFile =
        opts.logFile.value_or(LibrovaQt::QtRuntimePaths::DefaultLogFilePath(exeDir));
    const QString prefsFile =
        opts.preferencesFile.value_or(
            LibrovaQt::QtRuntimePaths::DefaultPreferencesFilePath(exeDir));
    const QString stateFile =
        opts.shellStateFile.value_or(
            LibrovaQt::QtRuntimePaths::DefaultShellStateFilePath(exeDir));

    // Ensure all runtime directories exist.
    QDir().mkpath(QFileInfo(logFile).absolutePath());
    QDir().mkpath(QFileInfo(prefsFile).absolutePath());
    QDir().mkpath(QFileInfo(stateFile).absolutePath());

    const auto logPath = Librova::Unicode::PathFromUtf8(logFile.toUtf8().toStdString());
    LibrovaQt::QtLogging::Initialize(logPath);

    // Print data paths to stdout for easy discovery during development.
    qInfo() << "Data root:" << QFileInfo(prefsFile).absolutePath();
    qInfo() << "Log file: " << logFile;

    LibrovaQt::PreferencesStore preferences;
    LibrovaQt::ShellStateStore shellState;
    if (!preferences.Load(prefsFile))
        qWarning() << "Could not load preferences; using defaults";
    if (!shellState.Load(stateFile))
        qWarning() << "Could not load shell state; using defaults";

    LibrovaQt::QtApplicationBackend backend;
    LibrovaQt::ShellController shellController(&preferences, &backend);
    LibrovaQt::QtCatalogAdapter catalogAdapter(&backend);
    LibrovaQt::QtExportAdapter  exportAdapter(&backend);
    LibrovaQt::QtImportAdapter  importAdapter(
        &backend,
        [&preferences, exeDir]() {
            return LibrovaQt::QtRuntimePaths::RuntimeWorkspaceRoot(
                       Librova::Unicode::PathFromUtf8(
                           preferences.libraryRoot().toUtf8().toStdString()),
                       exeDir) /
                   "ImportWorkspace";
        });
    LibrovaQt::QtTrashAdapter   trashAdapter(&backend);
    LibrovaQt::ConverterValidationController converterValidator;
    LibrovaQt::FirstRunController firstRunController;
    LibrovaQt::QtWindowsPlatform windowsPlatform;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(
        QStringLiteral("shellController"), &shellController);
    engine.rootContext()->setContextProperty(
        QStringLiteral("preferences"), &preferences);
    engine.rootContext()->setContextProperty(
        QStringLiteral("shellState"), &shellState);
    engine.rootContext()->setContextProperty(
        QStringLiteral("catalogAdapter"), &catalogAdapter);
    engine.rootContext()->setContextProperty(
        QStringLiteral("exportAdapter"), &exportAdapter);
    engine.rootContext()->setContextProperty(
        QStringLiteral("importAdapter"), &importAdapter);
    engine.rootContext()->setContextProperty(
        QStringLiteral("trashAdapter"), &trashAdapter);
    engine.rootContext()->setContextProperty(
        QStringLiteral("converterValidator"), &converterValidator);
    engine.rootContext()->setContextProperty(
        QStringLiteral("firstRunController"), &firstRunController);
    engine.rootContext()->setContextProperty(
        QStringLiteral("windowsPlatform"), &windowsPlatform);

    const QUrl mainQml(QStringLiteral("qrc:/qt/qml/LibrovaQt/qml/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.load(mainQml);

    // Bind the main window for DWM dark title bar and taskbar progress.
    if (!engine.rootObjects().isEmpty())
    {
        if (auto* win = qobject_cast<QWindow*>(engine.rootObjects().first()))
            windowsPlatform.setWindow(win);
    }

    // Update taskbar progress whenever an import job progresses.
    QObject::connect(&importAdapter, &LibrovaQt::QtImportAdapter::jobUpdated,
        [&windowsPlatform, &importAdapter](quint64) {
            const auto* model = importAdapter.importJobListModel();
            if (!model || model->rowCount() == 0)
            {
                windowsPlatform.updateTaskbarProgress(0, false);
                return;
            }
            // Use the most recent job (last row).
            const QModelIndex idx = model->index(model->rowCount() - 1);
            const bool terminal   = model->data(idx, LibrovaQt::ImportJobListModel::IsTerminalRole).toBool();
            if (terminal)
            {
                windowsPlatform.updateTaskbarProgress(0, false);
                return;
            }
            const int percent = model->data(idx, LibrovaQt::ImportJobListModel::PercentRole).toInt();
            windowsPlatform.updateTaskbarProgress(percent, true);
        });

    QObject::connect(&importAdapter, &LibrovaQt::QtImportAdapter::importCompleted,
        [&windowsPlatform](quint64, bool, const QString&) {
            windowsPlatform.updateTaskbarProgress(0, false);
        });

    shellController.start(opts);

    const int result = app.exec();

    if (preferences.HasLibraryRoot())
    {
        LibrovaQt::QtRuntimePaths::SyncRuntimeLogFile(
            LibrovaQt::QtLogging::CurrentLogFilePath(),
            LibrovaQt::QtRuntimePaths::UiLogFilePathForLibrary(
                Librova::Unicode::PathFromUtf8(preferences.libraryRoot().toUtf8().toStdString())));
    }

    backend.close();
    if (!preferences.Save(prefsFile))
        qWarning() << "Could not save preferences";
    if (!shellState.Save(stateFile))
        qWarning() << "Could not save shell state";
    LibrovaQt::QtLogging::Shutdown();

    return result;
}


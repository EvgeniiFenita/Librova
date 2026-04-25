#include "App/QtLaunchOptions.hpp"

#include <QCommandLineOption>
#include <QCommandLineParser>

namespace LibrovaQt {

SQtLaunchOptions SQtLaunchOptions::Parse(const QStringList& args)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Librova"));

    QCommandLineOption firstRunOpt(
        QStringLiteral("first-run"), QStringLiteral("Start in first-run setup mode."));
    QCommandLineOption recoveryOpt(
        QStringLiteral("startup-error-recovery"),
        QStringLiteral("Start in startup error recovery mode."));
    QCommandLineOption secondRunOpt(
        QStringLiteral("second-run"), QStringLiteral("Second run after setup."));
    QCommandLineOption libraryRootOpt(
        QStringLiteral("library-root"),
        QStringLiteral("Override library root for this session."),
        QStringLiteral("path"));
    QCommandLineOption createLibraryRootOpt(
        QStringLiteral("create-library-root"),
        QStringLiteral("Create the --library-root path as a new managed library."));
    QCommandLineOption logFileOpt(
        QStringLiteral("log-file"),
        QStringLiteral("Override log file path."),
        QStringLiteral("path"));
    QCommandLineOption prefsFileOpt(
        QStringLiteral("preferences-file"),
        QStringLiteral("Override preferences file path."),
        QStringLiteral("path"));
    QCommandLineOption stateFileOpt(
        QStringLiteral("shell-state-file"),
        QStringLiteral("Override shell state file path."),
        QStringLiteral("path"));

    parser.addOption(firstRunOpt);
    parser.addOption(recoveryOpt);
    parser.addOption(secondRunOpt);
    parser.addOption(libraryRootOpt);
    parser.addOption(createLibraryRootOpt);
    parser.addOption(logFileOpt);
    parser.addOption(prefsFileOpt);
    parser.addOption(stateFileOpt);

    // parse() does not terminate on --help/--version; safe without QCoreApplication.
    parser.parse(args);

    SQtLaunchOptions opts;
    opts.firstRun = parser.isSet(firstRunOpt);
    opts.startupErrorRecovery = parser.isSet(recoveryOpt);
    opts.secondRun = parser.isSet(secondRunOpt);
    opts.createLibraryRoot = parser.isSet(createLibraryRootOpt);

    if (parser.isSet(libraryRootOpt))
    {
        opts.libraryRoot = parser.value(libraryRootOpt);
    }
    if (parser.isSet(logFileOpt))
    {
        opts.logFile = parser.value(logFileOpt);
    }
    if (parser.isSet(prefsFileOpt))
    {
        opts.preferencesFile = parser.value(prefsFileOpt);
    }
    if (parser.isSet(stateFileOpt))
    {
        opts.shellStateFile = parser.value(stateFileOpt);
    }

    return opts;
}

} // namespace LibrovaQt

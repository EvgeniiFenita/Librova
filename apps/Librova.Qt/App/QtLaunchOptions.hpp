#pragma once

#include <optional>

#include <QString>

namespace LibrovaQt {

struct SQtLaunchOptions
{
    bool firstRun = false;
    bool startupErrorRecovery = false;
    bool secondRun = false;
    bool createLibraryRoot = false;

    std::optional<QString> libraryRoot;
    std::optional<QString> logFile;
    std::optional<QString> preferencesFile;
    std::optional<QString> shellStateFile;

    // Does not require QCoreApplication. Safe to call before app.exec().
    [[nodiscard]] static SQtLaunchOptions Parse(const QStringList& args);
};

} // namespace LibrovaQt

#pragma once

#include <QDir>
#include <QString>

namespace LibrovaQt::Tests {

inline QString TestWorkspaceTemplate(const QString& name)
{
    const QString root = QDir(QStringLiteral(LIBROVA_SOURCE_DIR))
                             .filePath(QStringLiteral("out/test-workspaces/qt"));
    QDir().mkpath(root);
    return QDir(root).filePath(name + QStringLiteral("-XXXXXX"));
}

} // namespace LibrovaQt::Tests

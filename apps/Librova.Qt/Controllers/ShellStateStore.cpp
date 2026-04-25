#include "Controllers/ShellStateStore.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>

namespace LibrovaQt {

ShellStateStore::ShellStateStore(QObject* parent)
    : QObject(parent)
{
}

bool ShellStateStore::Load(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject())
    {
        return false;
    }

    const QJsonObject root = doc.object();
    if (root.contains(QLatin1String("windowX")))
        setWindowX(root[QLatin1String("windowX")].toInt(m_windowX));
    if (root.contains(QLatin1String("windowY")))
        setWindowY(root[QLatin1String("windowY")].toInt(m_windowY));
    if (root.contains(QLatin1String("windowWidth")))
        setWindowWidth(root[QLatin1String("windowWidth")].toInt(m_windowWidth));
    if (root.contains(QLatin1String("windowHeight")))
        setWindowHeight(root[QLatin1String("windowHeight")].toInt(m_windowHeight));
    if (root.contains(QLatin1String("maximized")))
        setMaximized(root[QLatin1String("maximized")].toBool(m_maximized));
    if (root.contains(QLatin1String("lastSection")))
        setLastSection(root[QLatin1String("lastSection")].toString(m_lastSection));
    if (root.contains(QLatin1String("allowProbableDuplicates")))
    {
        setAllowProbableDuplicates(
            root[QLatin1String("allowProbableDuplicates")].toBool(m_allowProbableDuplicates));
    }
    if (root.contains(QLatin1String("sourcePaths")) && root[QLatin1String("sourcePaths")].isArray())
    {
        QStringList paths;
        for (const auto item : root[QLatin1String("sourcePaths")].toArray())
        {
            const QString path = item.toString();
            if (!path.isEmpty())
            {
                paths.push_back(path);
            }
        }
        setSourcePaths(paths);
    }
    return true;
}

bool ShellStateStore::Save(const QString& filePath) const
{
    QJsonObject root;
    root[QLatin1String("windowX")] = m_windowX;
    root[QLatin1String("windowY")] = m_windowY;
    root[QLatin1String("windowWidth")] = m_windowWidth;
    root[QLatin1String("windowHeight")] = m_windowHeight;
    root[QLatin1String("maximized")] = m_maximized;
    root[QLatin1String("lastSection")] = m_lastSection;
    root[QLatin1String("allowProbableDuplicates")] = m_allowProbableDuplicates;
    QJsonArray sourcePaths;
    for (const auto& path : m_sourcePaths)
    {
        sourcePaths.push_back(path);
    }
    root[QLatin1String("sourcePaths")] = sourcePaths;

    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }

    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0)
    {
        file.cancelWriting();
        return false;
    }

    return file.commit();
}

int ShellStateStore::windowX() const { return m_windowX; }
void ShellStateStore::setWindowX(int value)
{
    if (m_windowX != value) { m_windowX = value; Q_EMIT windowXChanged(); }
}

int ShellStateStore::windowY() const { return m_windowY; }
void ShellStateStore::setWindowY(int value)
{
    if (m_windowY != value) { m_windowY = value; Q_EMIT windowYChanged(); }
}

int ShellStateStore::windowWidth() const { return m_windowWidth; }
void ShellStateStore::setWindowWidth(int value)
{
    if (m_windowWidth != value) { m_windowWidth = value; Q_EMIT windowWidthChanged(); }
}

int ShellStateStore::windowHeight() const { return m_windowHeight; }
void ShellStateStore::setWindowHeight(int value)
{
    if (m_windowHeight != value) { m_windowHeight = value; Q_EMIT windowHeightChanged(); }
}

bool ShellStateStore::maximized() const { return m_maximized; }
void ShellStateStore::setMaximized(bool value)
{
    if (m_maximized != value) { m_maximized = value; Q_EMIT maximizedChanged(); }
}

QString ShellStateStore::lastSection() const { return m_lastSection; }
void ShellStateStore::setLastSection(const QString& value)
{
    if (m_lastSection != value) { m_lastSection = value; Q_EMIT lastSectionChanged(); }
}

bool ShellStateStore::allowProbableDuplicates() const
{
    return m_allowProbableDuplicates;
}

void ShellStateStore::setAllowProbableDuplicates(bool value)
{
    if (m_allowProbableDuplicates != value)
    {
        m_allowProbableDuplicates = value;
        Q_EMIT allowProbableDuplicatesChanged();
    }
}

QStringList ShellStateStore::sourcePaths() const
{
    return m_sourcePaths;
}

void ShellStateStore::setSourcePaths(const QStringList& value)
{
    if (m_sourcePaths != value)
    {
        m_sourcePaths = value;
        Q_EMIT sourcePathsChanged();
    }
}

} // namespace LibrovaQt

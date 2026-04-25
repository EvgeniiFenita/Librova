#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace LibrovaQt {

// Persists non-preference shell state (window geometry, last section).
class ShellStateStore : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int windowX READ windowX WRITE setWindowX NOTIFY windowXChanged)
    Q_PROPERTY(int windowY READ windowY WRITE setWindowY NOTIFY windowYChanged)
    Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowWidthChanged)
    Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowHeightChanged)
    Q_PROPERTY(bool maximized READ maximized WRITE setMaximized NOTIFY maximizedChanged)
    Q_PROPERTY(QString lastSection READ lastSection WRITE setLastSection NOTIFY lastSectionChanged)
    Q_PROPERTY(
        bool allowProbableDuplicates READ allowProbableDuplicates WRITE setAllowProbableDuplicates
            NOTIFY allowProbableDuplicatesChanged)
    Q_PROPERTY(QStringList sourcePaths READ sourcePaths WRITE setSourcePaths NOTIFY sourcePathsChanged)

public:
    explicit ShellStateStore(QObject* parent = nullptr);

    [[nodiscard]] bool Load(const QString& filePath);
    [[nodiscard]] bool Save(const QString& filePath) const;

    [[nodiscard]] int windowX() const;
    void setWindowX(int value);

    [[nodiscard]] int windowY() const;
    void setWindowY(int value);

    [[nodiscard]] int windowWidth() const;
    void setWindowWidth(int value);

    [[nodiscard]] int windowHeight() const;
    void setWindowHeight(int value);

    [[nodiscard]] bool maximized() const;
    void setMaximized(bool value);

    [[nodiscard]] QString lastSection() const;
    void setLastSection(const QString& value);

    [[nodiscard]] bool allowProbableDuplicates() const;
    void setAllowProbableDuplicates(bool value);

    [[nodiscard]] QStringList sourcePaths() const;
    void setSourcePaths(const QStringList& value);

Q_SIGNALS:
    void windowXChanged();
    void windowYChanged();
    void windowWidthChanged();
    void windowHeightChanged();
    void maximizedChanged();
    void lastSectionChanged();
    void allowProbableDuplicatesChanged();
    void sourcePathsChanged();

private:
    int m_windowX = 100;
    int m_windowY = 100;
    int m_windowWidth = 1280;
    int m_windowHeight = 800;
    bool m_maximized = false;
    QString m_lastSection = QStringLiteral("library");
    bool m_allowProbableDuplicates = false;
    QStringList m_sourcePaths;
};

} // namespace LibrovaQt

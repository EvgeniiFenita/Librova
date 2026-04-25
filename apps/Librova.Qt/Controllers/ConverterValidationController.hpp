#pragma once

#include <QObject>
#include <QString>

class QProcess;
class QTimer;

namespace LibrovaQt {

// Validates a converter executable asynchronously by running it with --version.
//
// Status values:
//   "none"     — no path has been submitted for validation
//   "checking" — validation in progress
//   "ok"       — exe ran successfully; versionString holds the first output line
//   "invalid"  — exe failed, timed out, or could not be started
class ConverterValidationController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString versionString READ versionString NOTIFY statusChanged)

public:
    explicit ConverterValidationController(QObject* parent = nullptr);
    ~ConverterValidationController() override;

    [[nodiscard]] QString status() const;
    [[nodiscard]] QString versionString() const;

    Q_INVOKABLE void validate(const QString& exePath);
    Q_INVOKABLE void clear();

Q_SIGNALS:
    void statusChanged();

private:
    void cancelCurrent();
    void setResult(const QString& status, const QString& version = {});

    QProcess* m_process = nullptr;
    QTimer*   m_timeout = nullptr;
    bool      m_validating = false;
    QString   m_status = QStringLiteral("none");
    QString   m_versionString;
};

} // namespace LibrovaQt

#include "Controllers/ConverterValidationController.hpp"

#include <QProcess>
#include <QTimer>

namespace LibrovaQt {

ConverterValidationController::ConverterValidationController(QObject* parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(5000);

    QObject::connect(m_timeout, &QTimer::timeout, this, [this]() {
        cancelCurrent();
        setResult(QStringLiteral("invalid"));
    });

    QObject::connect(
        m_process,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this,
        [this](int exitCode, QProcess::ExitStatus) {
            if (!m_validating)
                return;
            m_validating = false;
            m_timeout->stop();
            if (exitCode == 0) {
                const QString out =
                    QString::fromUtf8(m_process->readAllStandardOutput()).trimmed();
                const QString line =
                    out.split(u'\n', Qt::SkipEmptyParts).value(0).trimmed();
                setResult(
                    QStringLiteral("ok"),
                    line.isEmpty() ? QStringLiteral("ok") : line);
            } else {
                setResult(QStringLiteral("invalid"));
            }
        });

    QObject::connect(
        m_process,
        &QProcess::errorOccurred,
        this,
        [this](QProcess::ProcessError) {
            if (!m_validating)
                return;
            m_validating = false;
            m_timeout->stop();
            setResult(QStringLiteral("invalid"));
        });
}

ConverterValidationController::~ConverterValidationController() = default;

QString ConverterValidationController::status() const
{
    return m_status;
}

QString ConverterValidationController::versionString() const
{
    return m_versionString;
}

void ConverterValidationController::validate(const QString& exePath)
{
    if (exePath.isEmpty()) {
        clear();
        return;
    }
    cancelCurrent();
    m_validating = true;
    setResult(QStringLiteral("checking"));
    m_process->start(exePath, QStringList() << QStringLiteral("--version"));
    m_timeout->start();
}

void ConverterValidationController::clear()
{
    cancelCurrent();
    setResult(QStringLiteral("none"));
}

void ConverterValidationController::cancelCurrent()
{
    m_validating = false;
    m_timeout->stop();
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void ConverterValidationController::setResult(const QString& status, const QString& version)
{
    m_status       = status;
    m_versionString = version;
    Q_EMIT statusChanged();
}

} // namespace LibrovaQt

#include "DownloaderWidget.h"

#include <QBoxLayout>
#include <QObject>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>
#include <utility>

DownloaderWidget::DownloaderWidget(QWidget* parent)
    : QWidget(parent),
      textBox(new QPlainTextEdit(this)),
      startBtn(new QPushButton("Start downloading", this)),
      stopBtn(new QPushButton("Stop downloading", this)),
      process(new QProcess(this)),
      logTimer(new QTimer(this)){
    auto* layout = new QVBoxLayout(this);

    textBox->setReadOnly(true);
    textBox->setLineWrapMode(QPlainTextEdit::NoWrap);
    textBox->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    textBox->setStyleSheet(
        "background-color: black;"
        "color: white;");
    layout->addWidget(textBox, 1);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(startBtn);
    buttonLayout->addWidget(stopBtn);
    layout->addLayout(buttonLayout);

    connect(process, &QProcess::finished, this, &DownloaderWidget::handleProcessFinished);

    connect(process, &QProcess::errorOccurred, this, &DownloaderWidget::handleProcessError);

    connect(process, &QProcess::readyReadStandardError, this, &DownloaderWidget::handleStdErr);

    connect(logTimer, &QTimer::timeout, this, &DownloaderWidget::updateLogDisplay);

    connect(startBtn, &QPushButton::pressed, this, &DownloaderWidget::startDownloading);
    connect(stopBtn, &QPushButton::pressed, this, &DownloaderWidget::stopDownloading);
}

void DownloaderWidget::setDir(QString dir) { this->dir = std::move(dir); }

void DownloaderWidget::startDownloading() {
    QString basePath = QCoreApplication::applicationDirPath();
    QString exePath = QDir(basePath).filePath("downloader");

    if (!QFileInfo::exists(exePath) || !QFileInfo(exePath).isExecutable()) {
        QMessageBox::critical(this, "Error", "Downloader executable not found:\n" + exePath);
        return;
    }

    textBox->clear();
    lastLogPosition = 0;

    QString logFilePath = QDir(dir).filePath(logFileName);
    QFile logFile(logFilePath);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logFile.close();
    }

    logTimer->start(100);
    process->setWorkingDirectory(dir);
    process->start(exePath, {"--base-dir", dir});

    updateUiState(true);
}

void DownloaderWidget::stopDownloading() {
    logTimer->stop();
    if (process->state() == QProcess::Running) {
        process->kill();
        process->waitForFinished();
    }

    updateUiState(false);
}

void DownloaderWidget::handleProcessFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(exitCode)
    Q_UNUSED(status)

    logTimer->stop();
    updateLogDisplay();
    updateUiState(false);

    textBox->appendPlainText("\nProcess finished.\n");
}

void DownloaderWidget::handleProcessError(QProcess::ProcessError error) {
    Q_UNUSED(error)

    logTimer->stop();
    updateUiState(false);

    textBox->appendPlainText("\nProcess error occurred.\n");
}

void DownloaderWidget::handleStdErr() {
    QByteArray errorOutput = process->readAllStandardError();
    qDebug() << "Downloader stderr:" << QString::fromUtf8(errorOutput);
}

void DownloaderWidget::updateLogDisplay() {
    QString logFilePath = QDir(dir).filePath(logFileName);
    QFile logFile(logFilePath);

    if (!logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    logFile.seek(lastLogPosition);
    QByteArray newLines = logFile.readAll();
    lastLogPosition = logFile.pos();
    logFile.close();

    if (!newLines.isEmpty()) {
        textBox->appendPlainText(QString::fromUtf8(newLines));
    }
}

void DownloaderWidget::updateUiState(bool running) {
    startBtn->setEnabled(!running);
    stopBtn->setEnabled(running);
}

DownloaderWidget::~DownloaderWidget() {
    logTimer->stop();
    if (process->state() == QProcess::Running) {
        process->kill();
        process->waitForFinished();
    }
}
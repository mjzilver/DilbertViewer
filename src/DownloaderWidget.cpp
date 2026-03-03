#include "DownloaderWidget.h"

#include <qboxlayout.h>
#include <qcontainerfwd.h>
#include <qobject.h>
#include <qplaintextedit.h>
#include <qprocess.h>
#include <qpushbutton.h>

#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>
#include <utility>

DownloaderWidget::DownloaderWidget(QWidget* parent)
    : QWidget(parent),
      textBox(new QPlainTextEdit(this)),
      startBtn(new QPushButton("Start downloading", this)),
      stopBtn(new QPushButton("Stop downloading", this)),
      process(new QProcess(this)) {
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

    connect(process, &QProcess::readyReadStandardOutput, this, &DownloaderWidget::handleStdOut);

    connect(process, &QProcess::readyReadStandardError, this, &DownloaderWidget::handleStdErr);

    connect(process, &QProcess::finished, this, &DownloaderWidget::handleProcessFinished);

    connect(process, &QProcess::errorOccurred, this, &DownloaderWidget::handleProcessError);

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

    process->start(exePath, {"--base-dir", dir});
}

void DownloaderWidget::stopDownloading() {
    if (process->state() == QProcess::Running) {
        process->kill();
        process->waitForFinished();
    }

    updateUiState(false);
}

void DownloaderWidget::handleStdOut() {}

void DownloaderWidget::handleStdErr() {}

void DownloaderWidget::handleProcessFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(exitCode)
    Q_UNUSED(status)

    updateUiState(false);

    textBox->appendPlainText("\nProcess finished.\n");
}

void DownloaderWidget::handleProcessError(QProcess::ProcessError error) {
    Q_UNUSED(error)

    updateUiState(false);

    textBox->appendPlainText("\nProcess error occurred.\n");
}

void DownloaderWidget::appendOutput(const QString& text) {}

void DownloaderWidget::updateUiState(bool running) {
    startBtn->setEnabled(!running);
    stopBtn->setEnabled(running);
}

DownloaderWidget::~DownloaderWidget() {
    if (process->state() == QProcess::Running) {
        process->kill();
        process->waitForFinished();
    }
}
#pragma once

#include <QDebug>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QTextEdit>
#include <QTimer>
#include <QWidget>

class DownloaderWidget : public QWidget {
    Q_OBJECT

public:
    explicit DownloaderWidget(QWidget* parent = nullptr);
    void setDir(QString dir);
    ~DownloaderWidget();
private slots:
    void startDownloading();
    void stopDownloading();

    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void handleProcessError(QProcess::ProcessError error);
    void handleStdErr();
    void updateLogDisplay();

private:
    void appendOutput(const QString& text);
    void updateUiState(bool running);

    const QString logFileName = "dilbert_downloader.log";

    QPlainTextEdit* textBox;
    QPushButton* startBtn;
    QPushButton* stopBtn;

    QProcess* process;
    QProgressBar* progressBar;
    QTimer* logTimer;
    qint64 lastLogPosition = 0;

    QString dir;
};

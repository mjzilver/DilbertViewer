#pragma once

#include <qcontainerfwd.h>

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QTextEdit>
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

    void handleStdOut();
    void handleStdErr();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void handleProcessError(QProcess::ProcessError error);

private:
    void appendOutput(const QString& text);
    void updateUiState(bool running);

    QPlainTextEdit* textBox;
    QPushButton* startBtn;
    QPushButton* stopBtn;

    QProcess* process;
    QProgressBar* progressBar;
    QString stdoutBuffer;

    QString dir;
};

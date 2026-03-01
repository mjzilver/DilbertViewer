#pragma once

#include <QWidget>

class QLineEdit;
class QPushButton;

class SettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit SettingsWidget(QWidget* parent = nullptr);
    void setValues(const QString& dbPath, const QString& dilbertDir);

signals:
    void settingsChanged(const QString& dbPath, const QString& dilbertDir);

private slots:
    void browseDb();
    void browseDir();
    void apply();

private:
    QLineEdit* dbEdit;
    QLineEdit* dirEdit;
    QPushButton* applyBtn;
};

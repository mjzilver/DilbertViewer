#include "SettingsWidget.h"

#include <QBoxLayout>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

SettingsWidget::SettingsWidget(QWidget* parent)
    : QWidget(parent), dbEdit(new QLineEdit(this)), dirEdit(new QLineEdit(this)) {
    auto* layout = new QVBoxLayout(this);

    auto* dbLayout = new QHBoxLayout();
    dbLayout->addWidget(new QLabel("Database file:"));
    dbLayout->addWidget(dbEdit);

    auto* dbBtn = new QPushButton("Browse...", this);
    dbLayout->addWidget(dbBtn);
    layout->addLayout(dbLayout);

    auto* dirLayout = new QHBoxLayout();
    dirLayout->addWidget(new QLabel("Dilbert directory:"));
    dirLayout->addWidget(dirEdit);

    auto* dirBtn = new QPushButton("Browse...", this);
    dirLayout->addWidget(dirBtn);
    layout->addLayout(dirLayout);

    layout->addStretch();
    applyBtn = new QPushButton("Apply", this);
    layout->addWidget(applyBtn);

    connect(dbBtn, &QPushButton::clicked, this, &SettingsWidget::browseDb);
    connect(dirBtn, &QPushButton::clicked, this, &SettingsWidget::browseDir);
    connect(applyBtn, &QPushButton::clicked, this, &SettingsWidget::apply);
}

void SettingsWidget::setValues(const QString& dbPath, const QString& dilbertDir) {
    dbEdit->setText(dbPath);
    dirEdit->setText(dilbertDir);
}

void SettingsWidget::browseDb() {
    const QString fn = QFileDialog::getOpenFileName(this, "Select database file", dbEdit->text(),
                                                    "SQLite DB (*.db);;All Files (*)");
    if (!fn.isEmpty()) dbEdit->setText(fn);
}

void SettingsWidget::browseDir() {
    const QString dir =
        QFileDialog::getExistingDirectory(this, "Select Dilbert directory", dirEdit->text());
    if (!dir.isEmpty()) dirEdit->setText(dir);
}

void SettingsWidget::apply() { emit settingsChanged(dbEdit->text(), dirEdit->text()); }

#include "ComicViewerWidget.h"

#include <qlineedit.h>

#include <QDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

ComicViewerWidget::ComicViewerWidget(QWidget* parent) : QWidget(parent) {
    title = new QLabel("No comic");
    title->setAlignment(Qt::AlignCenter);
    title->setFont(QFont("Arial", 24, QFont::Bold));

    image = new QLabel;
    image->setAlignment(Qt::AlignCenter);
    image->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QScreen* screen = QGuiApplication::primaryScreen();
    QSize screenSize = screen->availableGeometry().size();
    image->setMaximumSize(screenSize.width() * 0.8, screenSize.height() * 0.6);

    tagLayout = new QHBoxLayout;
    QWidget* tagWidget = new QWidget;
    tagWidget->setLayout(tagLayout);

    tagWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* prev = new QPushButton("Previous");
    auto* rand = new QPushButton("Random");
    auto* next = new QPushButton("Next");

    connect(prev, &QPushButton::clicked, this, &ComicViewerWidget::previousRequested);
    connect(rand, &QPushButton::clicked, this, &ComicViewerWidget::randomRequested);
    connect(next, &QPushButton::clicked, this, &ComicViewerWidget::nextRequested);

    auto* nav = new QHBoxLayout;
    nav->addWidget(prev);
    nav->addWidget(rand);
    nav->addWidget(next);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(image);
    layout->addWidget(tagWidget);
    layout->addLayout(nav);
}

void ComicViewerWidget::showComic(const QDate& date, const QPixmap& pixmap) {
    current = pixmap;
    title->setText("Dilbert: " + date.toString(Qt::ISODate));
    image->setPixmap(current.scaled(image->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void ComicViewerWidget::resizeEvent(QResizeEvent*) {
    if (!current.isNull())
        image->setPixmap(
            current.scaled(image->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void ComicViewerWidget::setTags(const QStringList& tags) {
    QLayoutItem* item;
    while ((item = tagLayout->takeAt(0))) {
        delete item->widget();
        delete item;
    }

    if (tags.isEmpty()) {
        auto* label = new QLabel("No tags found");
        label->setEnabled(false);
        label->setStyleSheet("color: gray;");
        tagLayout->addWidget(label);
        return;
    }

    for (const QString& tag : tags) {
        auto* btn = new QPushButton(tag);
        btn->setFlat(true);
        btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        connect(btn, &QPushButton::clicked, this, [this, tag] { emit tagSelected(tag); });
        tagLayout->addWidget(btn);
    }

    auto* editBtn = new QPushButton("Edit tags");
    connect(editBtn, &QPushButton::clicked, this, [this, tags] { emit editTags(tags); });
    tagLayout->addWidget(editBtn);
}

void ComicViewerWidget::editTags(const QStringList& tags) {
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle("Edit Tags");

    QVBoxLayout* layout = new QVBoxLayout(dialog);

    for (const QString& tag : tags) {
        QHBoxLayout* rowLayout = new QHBoxLayout;
        QLineEdit* lineEdit = new QLineEdit(tag);
        QPushButton* saveBtn = new QPushButton("Save");

        rowLayout->addWidget(lineEdit);
        rowLayout->addWidget(saveBtn);

        connect(saveBtn, &QPushButton::clicked, this, [this, lineEdit, &tag]() {
            auto newTag = lineEdit->text();
            emit tagEdited(tag, newTag);
        });

        layout->addLayout(rowLayout);
    }

    QPushButton* closeBtn = new QPushButton("Close");
    layout->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);

    dialog->setLayout(layout);
    dialog->setModal(false);
    dialog->show();
}

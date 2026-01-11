#include "ComicViewerWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ComicViewerWidget::ComicViewerWidget(QWidget* parent) : QWidget(parent) {
    title = new QLabel("No comic");
    title->setAlignment(Qt::AlignCenter);
    title->setFont(QFont("Arial", 24, QFont::Bold));

    image = new QLabel;
    image->setAlignment(Qt::AlignCenter);
    image->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    tagLayout = new QHBoxLayout;

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
    layout->addLayout(tagLayout);
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
        connect(btn, &QPushButton::clicked, this, [this, tag] { emit tagSelected(tag); });
        tagLayout->addWidget(btn);
    }
}

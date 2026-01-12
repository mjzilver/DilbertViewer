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

ComicViewerWidget::ComicViewerWidget(QWidget* parent, ComicTagsWidget* tags) : QWidget(parent) {
    title = new QLabel("No comic");
    title->setAlignment(Qt::AlignCenter);
    title->setFont(QFont("Arial", 24, QFont::Bold));

    image = new QLabel;
    image->setAlignment(Qt::AlignCenter);
    image->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    image->setMaximumSize(maximumWidth() * 0.8, maximumHeight() * 0.6);

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
    layout->addWidget(tags);
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

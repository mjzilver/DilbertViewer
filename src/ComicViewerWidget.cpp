#include "ComicViewerWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScreen>
#include <QToolTip>
#include <QVBoxLayout>

#include "ComicTagsWidget.h"

ComicViewerWidget::ComicViewerWidget(QWidget* parent, ComicTagsWidget* tags)
    : QWidget(parent), imageLabel(new QLabel), nav(new QHBoxLayout) {
    titleLabel = new QLabel("No comic");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setFont(QFont("Arial", 24, QFont::Bold));

    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Ignored);
    imageLabel->setMinimumSize(400, 200);

    auto* prev = new QPushButton("Previous");
    auto* rand = new QPushButton("Random");
    auto* next = new QPushButton("Next");
    auto* edit = new QPushButton("Edit tags");

    connect(prev, &QPushButton::clicked, this, &ComicViewerWidget::previousRequested);
    connect(rand, &QPushButton::clicked, this, &ComicViewerWidget::randomRequested);
    connect(next, &QPushButton::clicked, this, &ComicViewerWidget::nextRequested);
    connect(edit, &QPushButton::clicked, tags, &ComicTagsWidget::openEditDialog);

    nav->addWidget(prev);
    nav->addWidget(rand);
    nav->addWidget(next);
    nav->addWidget(edit);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(titleLabel);
    layout->addWidget(imageLabel, 1);
    layout->addWidget(tags, 0, Qt::AlignBottom);
    layout->addLayout(nav);
}

void ComicViewerWidget::showComic(const QDate& date, const QPixmap& pixmap) {
    currentPixmap = pixmap;
    titleLabel->setText("Dilbert: " + date.toString(Qt::ISODate));
    imageLabel->setPixmap(
        currentPixmap.scaled(imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void ComicViewerWidget::resizeEvent(QResizeEvent*) {
    if (currentPixmap.isNull()) return;

    imageLabel->setPixmap(
        currentPixmap.scaled(imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void ComicViewerWidget::addButton(QPushButton* newBtn) { nav->addWidget(newBtn); }

void ComicViewerWidget::copyImageToClipboard() {
    if (currentPixmap.isNull()) return;

    QApplication::clipboard()->setPixmap(currentPixmap);

    QString text = "Image copied to clipboard";

    QPoint centerGlobal = imageLabel->mapToGlobal(imageLabel->rect().center());

    QFontMetrics fm(QToolTip::font());
    QRect textRect = fm.boundingRect(text);
    QSize size = textRect.size();

    QPoint topLeft = centerGlobal - QPoint(size.width() / 2, size.height() / 2);

    QToolTip::showText(topLeft, text, imageLabel);
}

#include "ComicSearchWidget.h"

#include <QComboBox>
#include <QCompleter>
#include <QFile>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QVBoxLayout>

ComicSearchWidget::ComicSearchWidget(const QStringList& tags, QWidget* parent) : QWidget(parent) {
    modeBox = new QComboBox;
    modeBox->addItems({"Tag", "Date", "Transcript"});

    edit = new QLineEdit;
    edit->setCompleter(new QCompleter(tags, this));

    auto* bar = new QHBoxLayout;
    bar->addWidget(modeBox);
    bar->addWidget(edit);

    gallery = new QListWidget;
    gallery->setViewMode(QListView::IconMode);
    gallery->setIconSize({150, 150});
    gallery->setSpacing(10);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(bar);
    layout->addWidget(gallery);

    connect(edit, &QLineEdit::returnPressed, this, &ComicSearchWidget::onReturnPressed);
    connect(gallery, &QListWidget::itemClicked, this, &ComicSearchWidget::onItemClicked);
}

void ComicSearchWidget::onReturnPressed() {
    emit searchRequested(edit->text().trimmed(), static_cast<Mode>(modeBox->currentIndex()));
}

void ComicSearchWidget::showResults(const QList<ComicItem>& comics) {
    gallery->clear();
    pending.clear();

    for (const ComicItem& comic : comics) pending.enqueue(comic);

    if (!thumbTimer.isActive()) {
        connect(&thumbTimer, &QTimer::timeout, this, &ComicSearchWidget::loadNextThumbnail,
                Qt::UniqueConnection);
    }

    thumbTimer.start(0);
}

void ComicSearchWidget::loadNextThumbnail() {
    if (pending.isEmpty()) {
        thumbTimer.stop();
        return;
    }

    const ComicItem comic = pending.dequeue();
    if (!QFile::exists(comic.path)) return;

    QPixmap pix(comic.path);
    auto* item = new QListWidgetItem(
        QIcon(pix.scaled(150, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation)), QString());

    item->setData(Qt::UserRole, comic.date);
    gallery->addItem(item);
}

void ComicSearchWidget::onItemClicked(QListWidgetItem* item) {
    emit comicSelected(item->data(Qt::UserRole).toDate());
}

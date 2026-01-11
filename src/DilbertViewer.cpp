#include "DilbertViewer.h"

#include <qboxlayout.h>
#include <qcompleter.h>
#include <qcontainerfwd.h>
#include <qdebug.h>
#include <qlogging.h>
#include <qsqlquery.h>
#include <qstyle.h>
#include <qwidget.h>

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QFile>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QRandomGenerator>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtSql>

QStringList DilbertViewer::getAllComicTags() {
    QStringList titles;

    if (!db.isOpen()) {
        return titles;
    }

    QSqlQuery query("SELECT name FROM tags ORDER BY name ASC");
    while (query.next()) {
        titles << query.value(0).toString();
    }

    return titles;
}

QStringList DilbertViewer::getTagsForCurrentComic() {
    QStringList tags;

    if (!db.isOpen()) return tags;

    QSqlQuery query(db);
    query.prepare(
        "SELECT tags.name FROM tags "
        "INNER JOIN comic_tags ON comic_tags.tag_id = tags.id "
        "INNER JOIN comics ON comics.date = comic_tags.comic_date "
        "WHERE comics.date = :date");
    query.bindValue(":date", currentComicDate.toString("yyyy-MM-dd"));

    if (!query.exec()) {
        qDebug() << "getTagsForCurrentComic - SQLERR: " << query.lastError().text();
        return tags;
    }

    while (query.next()) {
        tags << query.value(0).toString();
    }

    return tags;
}

QList<ComicItem> DilbertViewer::getComicsForTag(const QString& tag) {
    QList<ComicItem> comics;
    if (!db.isOpen()) return comics;

    QSqlQuery query(db);
    query.prepare(
        "SELECT comics.date, comics.image_path "
        "FROM comics "
        "INNER JOIN comic_tags ON comic_tags.comic_date = comics.date "
        "INNER JOIN tags ON tags.id = comic_tags.tag_id "
        "WHERE tags.name = :tag "
        "ORDER BY comics.date ASC");
    query.bindValue(":tag", tag);

    if (!query.exec()) {
        qDebug() << query.lastError().text();
        return comics;
    }

    while (query.next()) {
        ComicItem item;
        item.date = QDate::fromString(query.value(0).toString(), "yyyy-MM-dd");
        item.path = query.value(1).toString();
        comics.append(item);
    }

    return comics;
}

QList<ComicItem> DilbertViewer::getComicsForDate(const QString& dateStr) {
    QList<ComicItem> comics;

    if (!db.isOpen()) return comics;

    QSqlQuery query(db);
    query.prepare("SELECT date, image_path FROM comics WHERE date = :date");
    query.bindValue(":date", dateStr);

    if (!query.exec()) {
        qDebug() << query.lastError().text();
        return comics;
    }

   while (query.next()) {
        ComicItem item;
        item.date = QDate::fromString(query.value(0).toString(), "yyyy-MM-dd");
        item.path = query.value(1).toString();
        comics.append(item);
    }

    return comics;
}

QList<ComicItem> DilbertViewer::getComicsForTranscript(const QString& searchTerm) {
    QList<ComicItem> comics;

    if (!db.isOpen()) return comics;

    QSqlQuery query(db);
    query.prepare(
        "SELECT date, image_path FROM comics "
        "WHERE transcript LIKE :text");
    query.bindValue(":text", "%" + searchTerm + "%");

    if (!query.exec()) {
        qDebug() << query.lastError().text();
        return comics;
    }

    while (query.next()) {
        ComicItem item;
        item.date = QDate::fromString(query.value(0).toString(), "yyyy-MM-dd");
        item.path = query.value(1).toString();
        comics.append(item);
    }


    return comics;
}

DilbertViewer::DilbertViewer(QWidget* parent)
    : QMainWindow(parent), firstComicDate(1989, 4, 16), lastComicDate(2023, 3, 12) {
    setWindowTitle("Dilbert Viewer");
    resize(800, 600);

    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("./Dilbert/metadata.db");
    if (!db.open()) {
        qDebug() << "Failed to open database!";
    }

    QWidget* central = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    tabs = new QTabWidget(central);

    QWidget* viewerTab = new QWidget();
    QVBoxLayout* viewerLayout = new QVBoxLayout(viewerTab);

    titleLabel = new QLabel("No comic loaded");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setFont(QFont("Arial", 24, QFont::Bold));

    imageLabel = new QLabel();
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QWidget* buttonWidget = new QWidget();
    QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
    prevButton = new QPushButton("Previous");
    randButton = new QPushButton("Random");
    nextButton = new QPushButton("Next");

    buttonLayout->addWidget(prevButton);
    buttonLayout->addWidget(randButton);
    buttonLayout->addWidget(nextButton);

    QWidget* descriptionWidget = new QWidget();
    tagLayout = new QHBoxLayout(descriptionWidget);

    viewerLayout->addWidget(titleLabel);
    viewerLayout->addWidget(imageLabel);
    viewerLayout->addWidget(descriptionWidget);
    viewerLayout->addWidget(buttonWidget);

    tabs->addTab(viewerTab, "Viewer");

    QWidget* searchTab = new QWidget();
    QVBoxLayout* searchLayout = new QVBoxLayout(searchTab);

    searchModeBox = new QComboBox(searchTab);
    searchModeBox->addItems({"Tag", "Date", "Transcript"});

    QLineEdit* searchEdit = new QLineEdit(searchTab);
    searchEdit->setPlaceholderText("Search by tag...");

    QHBoxLayout* searchBarLayout = new QHBoxLayout();
    searchBarLayout->addWidget(searchModeBox);
    searchBarLayout->addWidget(searchEdit);

    searchLayout->addLayout(searchBarLayout);

    search = new QCompleter(getAllComicTags(), this);
    search->setCaseSensitivity(Qt::CaseInsensitive);
    searchEdit->setCompleter(search);

    currentSearchMode = SearchMode::Tag;

    connect(searchModeBox, &QComboBox::currentIndexChanged, this, [this, searchEdit](int index) {
        currentSearchMode = static_cast<SearchMode>(index);

        if (index == 0) {
            searchEdit->setPlaceholderText("Search by tag...");
            searchEdit->setCompleter(search);
        } else if (index == 1) {
            searchEdit->setPlaceholderText("Search by date (YYYY-MM-DD)...");
            searchEdit->setCompleter(nullptr);
        } else {
            searchEdit->setPlaceholderText("Search transcript text...");
            searchEdit->setCompleter(nullptr);
        }
    });

    galleryWidget = new QListWidget(searchTab);
    galleryWidget->setViewMode(QListView::IconMode);
    galleryWidget->setIconSize(QSize(150, 150));
    galleryWidget->setResizeMode(QListView::Adjust);
    galleryWidget->setSpacing(10);

    searchLayout->addWidget(galleryWidget);
    tabs->addTab(searchTab, "Search");

    mainLayout->addWidget(tabs);
    setCentralWidget(central);

    connect(randButton, &QPushButton::clicked, this, &DilbertViewer::showRandom);
    connect(prevButton, &QPushButton::clicked, this, &DilbertViewer::showPrevious);
    connect(nextButton, &QPushButton::clicked, this, &DilbertViewer::showNext);

    connect(searchEdit, &QLineEdit::returnPressed, this, &DilbertViewer::onSearchReturnPressed);
    connect(galleryWidget, &QListWidget::itemClicked, this, &DilbertViewer::onGalleryItemClicked);

    QTimer::singleShot(0, this, &DilbertViewer::showRandom);
}

void DilbertViewer::onSearchReturnPressed() {
    QLineEdit* edit = qobject_cast<QLineEdit*>(sender());
    if (!edit) return;

    QString query = edit->text().trimmed();
    performSearch(query, currentSearchMode);
}

void DilbertViewer::performSearch(const QString& query, SearchMode mode) {
    if (query.isEmpty()) return;

    galleryWidget->clear();
    QList<ComicItem> comics;

    switch (mode) {
        case SearchMode::Tag:
            comics = getComicsForTag(query);
            break;
        case SearchMode::Date:
            comics = getComicsForDate(query);
            break;
        case SearchMode::Transcript:
            comics = getComicsForTranscript(query);
            break;
    }

    for (const ComicItem& comic : comics) {
        QString fullPath = "./Dilbert/" + comic.path;
        if (!QFile::exists(fullPath)) continue;

        QPixmap pix(fullPath);
        auto* item = new QListWidgetItem(
            QIcon(pix.scaled(150, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation)), "");

        item->setData(Qt::UserRole, comic.date);

        galleryWidget->addItem(item);
    }

    tabs->setCurrentIndex(1);
}


void DilbertViewer::onGalleryItemClicked(QListWidgetItem* item) {
    QDate date = item->data(Qt::UserRole).toDate();
    if (date.isValid()) {
        loadAndDisplay(date);
        tabs->setCurrentIndex(0);
    }
}

QDate DilbertViewer::generateRandomDate() {
    int days = firstComicDate.daysTo(lastComicDate);
    return firstComicDate.addDays(QRandomGenerator::global()->bounded(days + 1));
}

QString DilbertViewer::generateFilePath(const QDate& date) {
    return QString("./Dilbert/%1/Dilbert_%1-%2-%3.png")
        .arg(date.year(), 4, 10, QChar('0'))
        .arg(date.month(), 2, 10, QChar('0'))
        .arg(date.day(), 2, 10, QChar('0'));
}

void DilbertViewer::loadAndDisplay(const QDate& date) {
    QString path = generateFilePath(date);
    while (!QFile::exists(path)) {
        path = generateFilePath(generateRandomDate());
    }

    currentComic.load(path);
    imageLabel->setPixmap(
        currentComic.scaled(imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    titleLabel->setText(QString("Dilbert: %1").arg(date.toString("yyyy-MM-dd")));
    currentComicDate = date;

    updateTagButtons();
}

void DilbertViewer::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    if (!currentComic.isNull()) {
        imageLabel->setPixmap(
            currentComic.scaled(imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void DilbertViewer::updateTagButtons() {
    QLayoutItem* child;
    while ((child = tagLayout->takeAt(0)) != nullptr) {
        if (QWidget* w = child->widget()) {
            w->deleteLater();
        }
        delete child;
    }

    QStringList tags = getTagsForCurrentComic();
    for (const QString& tag : tags) {
        QPushButton* btn = new QPushButton(tag);
        btn->setStyleSheet("QPushButton { padding: 2px 6px; margin: 0px; }");
        btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        btn->setFlat(true);

        connect(btn, &QPushButton::clicked, this, [this, tag]() {
            currentSearchMode = SearchMode::Tag;
            searchModeBox->setCurrentIndex(static_cast<int>(SearchMode::Tag));
            performSearch(tag, currentSearchMode);
        });

        tagLayout->addWidget(btn);
    }
}

void DilbertViewer::showRandom() { loadAndDisplay(generateRandomDate()); }

void DilbertViewer::showPrevious() {
    if (currentComicDate > firstComicDate) loadAndDisplay(currentComicDate.addDays(-1));
}
void DilbertViewer::showNext() {
    if (currentComicDate < lastComicDate) loadAndDisplay(currentComicDate.addDays(1));
}

DilbertViewer::~DilbertViewer() {
    if (db.isOpen()) db.close();
}

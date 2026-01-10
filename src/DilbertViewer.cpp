#include "DilbertViewer.h"

#include <qboxlayout.h>
#include <qcompleter.h>
#include <qdebug.h>
#include <qlogging.h>
#include <qsqlquery.h>

#include <QApplication>
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
        qDebug() << "getAllComicTags could not connect to db";
        return titles;
    }

    QSqlQuery query("SELECT name FROM tags ORDER BY name ASC");
    while (query.next()) {
        titles << query.value(0).toString();
    }

    qDebug() << titles.length() << " tags found";

    return titles;
}

QStringList DilbertViewer::getImagePathsForTag(const QString& tag) {
    QStringList paths;

    if (!db.isOpen()) {
        qDebug() << "getImagePathsForTag could not connect to db";
        return paths;
    }

    QSqlQuery query(db);
    query.prepare(
        "SELECT comics.image_path "
        "FROM comics "
        "INNER JOIN comic_tags ON comic_tags.comic_date = comics.date "
        "INNER JOIN tags ON tags.id = comic_tags.tag_id "
        "WHERE tags.name = :tag "
        "ORDER BY comics.date ASC");

    query.bindValue(":tag", tag);

    if (!query.exec()) {
        qDebug() << "Query failed:" << query.lastError().text();
        return paths;
    }

    while (query.next()) {
        paths << query.value(0).toString();
    }

    qDebug() << paths.length() << " images found for " << tag;

    return paths;
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

    titleLabel = new QLabel("No comic loaded", viewerTab);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont("Arial", 24, QFont::Bold);
    titleLabel->setFont(titleFont);

    imageLabel = new QLabel(viewerTab);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setMinimumSize(400, 300);
    imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    imageLabel->setScaledContents(false);

    QWidget* buttonWidget = new QWidget(viewerTab);
    QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
    prevButton = new QPushButton("Previous", buttonWidget);
    randButton = new QPushButton("Random", buttonWidget);
    nextButton = new QPushButton("Next", buttonWidget);
    buttonLayout->addWidget(prevButton);
    buttonLayout->addWidget(randButton);
    buttonLayout->addWidget(nextButton);

    viewerLayout->addWidget(titleLabel);
    viewerLayout->addWidget(imageLabel);
    viewerLayout->addWidget(buttonWidget);

    tabs->addTab(viewerTab, "Viewer");

    QWidget* searchTab = new QWidget();
    QVBoxLayout* searchLayout = new QVBoxLayout(searchTab);

    QLineEdit* searchEdit = new QLineEdit(searchTab);
    searchEdit->setPlaceholderText("Search by tag...");

    search = new QCompleter(searchTab);
    search->setModel(new QStringListModel(getAllComicTags(), search));
    search->setCaseSensitivity(Qt::CaseInsensitive);
    searchEdit->setCompleter(search);

    searchLayout->addWidget(searchEdit);

    galleryWidget = new QListWidget(searchTab);
    galleryWidget->setViewMode(QListView::IconMode);
    galleryWidget->setIconSize(QSize(150, 150));
    galleryWidget->setResizeMode(QListView::Adjust);
    galleryWidget->setSpacing(10);
    galleryWidget->setMovement(QListView::Static);

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
    QLineEdit* searchEdit = qobject_cast<QLineEdit*>(sender());
    if (!searchEdit) return;

    QString tag = searchEdit->text();
    galleryWidget->clear();

    QStringList imagePaths = getImagePathsForTag(tag);

    for (const QString& path : imagePaths) {
        QString fullPath = QString("./Dilbert/%1").arg(path);
        if (!QFile::exists(fullPath))  {
            qDebug() << fullPath << " file does not exist";
            continue;
        };

        QPixmap pix(fullPath);
        QListWidgetItem* item = new QListWidgetItem(
            QIcon(pix.scaled(150, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation)), "");
        item->setData(Qt::UserRole, fullPath);
        galleryWidget->addItem(item);
    }
}

void DilbertViewer::onGalleryItemClicked(QListWidgetItem* item) {
    QString path = item->data(Qt::UserRole).toString();
    currentComic.load(path);
    imageLabel->setPixmap(
        currentComic.scaled(imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    titleLabel->setText(QFileInfo(path).baseName());

    tabs->setCurrentIndex(0);
}

QDate DilbertViewer::generateRandomDate() {
    int daysRange = firstComicDate.daysTo(lastComicDate);
    int randomOffset = QRandomGenerator::global()->bounded(daysRange + 1);
    return firstComicDate.addDays(randomOffset);
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
        qDebug() << "Comic does not exist:" << path;
        path = generateFilePath(generateRandomDate());
    }

    currentComic.load(path);
    imageLabel->setPixmap(
        currentComic.scaled(imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    titleLabel->setText(
        QString("Dilbert: %1-%2-%3").arg(date.year()).arg(date.month()).arg(date.day()));

    currentComicDate = date;
}

void DilbertViewer::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);

    if (!currentComic.isNull()) {
        imageLabel->setPixmap(
            currentComic.scaled(imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void DilbertViewer::showRandom() {
    QDate randomDate = generateRandomDate();
    loadAndDisplay(randomDate);
}

void DilbertViewer::showPrevious() {
    if (currentComicDate > firstComicDate) {
        loadAndDisplay(currentComicDate.addDays(-1));
    }
}

void DilbertViewer::showNext() {
    if (currentComicDate < lastComicDate) {
        loadAndDisplay(currentComicDate.addDays(1));
    }
}

DilbertViewer::~DilbertViewer() {
    if (db.isOpen()) {
        db.close();
    }
}
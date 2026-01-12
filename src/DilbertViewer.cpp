#include "DilbertViewer.h"

#include <qcontainerfwd.h>

#include <QFile>
#include <QGuiApplication>
#include <QPixmap>
#include <QRandomGenerator>
#include <QScreen>
#include <QTabWidget>

#include "ComicTagsWidget.h"
#include "ComicViewerWidget.h"

DilbertViewer::DilbertViewer(QWidget* parent) : QMainWindow(parent), repo("./Dilbert/metadata.db") {
    QScreen* screen = QGuiApplication::primaryScreen();
    QSize screenSize = screen->availableGeometry().size();

    auto* tabs = new QTabWidget(this);

    tags = new ComicTagsWidget();
    viewer = new ComicViewerWidget(this, tags);

    viewer->setMaximumSize(screenSize.width() * 0.8, screenSize.height() * 0.6);
    tags->setMaximumSize(screenSize.width() * 0.8, screenSize.height() * 0.1);

    search = new ComicSearchWidget(repo.allTags());

    tabs->addTab(viewer, "Viewer");
    tabs->addTab(search, "Search");

    setCentralWidget(tabs);
    resize(800, 600);

    connect(viewer, &ComicViewerWidget::previousRequested, this,
            [this] { loadComic(currentDate.addDays(-1)); });

    connect(viewer, &ComicViewerWidget::nextRequested, this,
            [this] { loadComic(currentDate.addDays(1)); });

    connect(viewer, &ComicViewerWidget::randomRequested, this, [this] { loadComic(randomDate()); });

    connect(tags, &ComicTagsWidget::tagSelected, this, [this, tabs](const QString& tag) {
        auto comics = repo.comicsForTag(tag);
        for (ComicItem& c : comics) c.path = "./Dilbert/" + c.path;

        search->showResults(comics);
        search->setInput(tag);
        tabs->setCurrentIndex(1);
    });

    connect(tags, &ComicTagsWidget::tagEdited, this,
            [this](const QString& oldTag, const QString& newTag) {
                repo.editTag(oldTag, newTag);
                tags->setTags(repo.tagsForComic(currentDate));
            });

    connect(search, &ComicSearchWidget::searchRequested, this,
            [this, tabs](const QString& q, ComicSearchWidget::Mode m) {
                QList<ComicItem> comics;

                switch (m) {
                    case ComicSearchWidget::Tag:
                        comics = repo.comicsForTag(q);
                        break;

                    case ComicSearchWidget::Date:
                        comics = repo.comicsForDate(q);
                        break;

                    case ComicSearchWidget::Transcript:
                        comics = repo.comicsForTranscript(q);
                        break;
                }

                for (ComicItem& c : comics) c.path = "./Dilbert/" + c.path;

                search->showResults(comics);
                tabs->setCurrentIndex(1);
            });

    connect(search, &ComicSearchWidget::comicSelected, this, [this, tabs](const QDate& d) {
        loadComic(d);
        tabs->setCurrentIndex(0);
    });

    loadComic(randomDate());
}

QDate DilbertViewer::randomDate() const {
    return first.addDays(QRandomGenerator::global()->bounded(first.daysTo(last)));
}

QString DilbertViewer::comicPath(const QDate& d) const {
    return QString("./Dilbert/%1/Dilbert_%1-%2-%3.png")
        .arg(d.year(), 4, 10, QChar('0'))
        .arg(d.month(), 2, 10, QChar('0'))
        .arg(d.day(), 2, 10, QChar('0'));
}

void DilbertViewer::loadComic(const QDate& date) {
    const QString path = comicPath(date);
    if (!QFile::exists(path)) return;

    QPixmap pix(path);
    currentDate = date;

    viewer->showComic(date, pix);
    tags->setTags(repo.tagsForComic(date));
}

#pragma once
#include <QDate>
#include <QMainWindow>

#include "ComicRepository.h"
#include "ComicSearchWidget.h"
#include "ComicTagsWidget.h"
#include "ComicViewerWidget.h"

class DilbertViewer : public QMainWindow {
    Q_OBJECT
public:
    explicit DilbertViewer(QWidget* parent = nullptr);

    void keyPressEvent(QKeyEvent* event) override;

private:
    void loadComic(const QDate& date);
    QDate randomDate() const;
    QString comicPath(const QDate& date) const;

    ComicRepository repo;
    ComicViewerWidget* viewer;
    ComicSearchWidget* search;
    ComicTagsWidget* tags;

    QDate currentDate;
    const QDate first{1989, 4, 16};
    const QDate last{2023, 3, 12};
};

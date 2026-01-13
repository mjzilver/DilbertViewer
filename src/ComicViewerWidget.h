#pragma once
#include <qpixmap.h>
#include <qpushbutton.h>

#include <QDate>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QWidget>

#include "ComicTagsWidget.h"

class ComicViewerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ComicViewerWidget(QWidget* parent = nullptr, ComicTagsWidget* tags = nullptr);

    void showComic(const QDate& date, const QPixmap& pixmap);
    void addButton(QPushButton* newBtn);

signals:
    void previousRequested();
    void nextRequested();
    void randomRequested();

protected:
    void resizeEvent(QResizeEvent*) override;

private:
    QLabel* title;
    QLabel* image;
    QPixmap current;
    QHBoxLayout* nav;
};

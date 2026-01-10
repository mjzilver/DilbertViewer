#pragma once

#include <qpixmap.h>
#include <QDate>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>

class DilbertViewer : public QMainWindow {
    Q_OBJECT

public:
    explicit DilbertViewer(QWidget *parent = nullptr);

private slots:
    void showRandom();
    void showPrevious();
    void showNext();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QLabel *titleLabel;
    QLabel *imageLabel;
    QPushButton *prevButton;
    QPushButton *randButton;
    QPushButton *nextButton;

    QDate firstComicDate;
    QDate lastComicDate;
    QDate currentComicDate;

    QPixmap currentComic;

    QDate generateRandomDate();
    QString generateFilePath(const QDate &date);
    void loadAndDisplay(const QDate &date);
};

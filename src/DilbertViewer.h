#pragma once

#include <qcontainerfwd.h>
#include <qpixmap.h>

#include <QComboBox>
#include <QCompleter>
#include <QDate>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QSql>
#include <QSqlDatabase>

class DilbertViewer : public QMainWindow {
    Q_OBJECT

public:
    explicit DilbertViewer(QWidget* parent = nullptr);
    ~DilbertViewer();

private slots:
    void onSearchReturnPressed();
    void onGalleryItemClicked(QListWidgetItem* item);

private:
    enum class SearchMode { Tag, Date, Transcript };

    QSqlDatabase db;

    QTabWidget* tabs;
    QCompleter* search;
    QListWidget* galleryWidget;
    QLabel* imageLabel;
    QLabel* titleLabel;
    QPushButton* prevButton;
    QPushButton* randButton;
    QPushButton* nextButton;
    QDate firstComicDate;
    QDate lastComicDate;
    QDate currentComicDate;
    QPixmap currentComic;
    QComboBox* searchModeBox;

    SearchMode currentSearchMode;

    QString generateFilePath(const QDate& date);
    QDate generateRandomDate();

    QStringList getAllComicTags();
    QStringList getImagePathsForTag(const QString& tag);
    QStringList getImagePathsForTranscript(const QString& searchTerm);
    QStringList getImagePathsForDate(const QString& dateStr);

    void resizeEvent(QResizeEvent* event) override;

    void loadAndDisplay(const QDate& date);
    void showRandom();
    void showPrevious();
    void showNext();
};

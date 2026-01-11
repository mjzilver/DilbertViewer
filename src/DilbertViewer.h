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
#include <QHBoxLayout>

struct ComicItem {
    QDate date;
    QString path;
};

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
    QHBoxLayout* tagLayout;

    SearchMode currentSearchMode;

    QString generateFilePath(const QDate& date);
    QDate generateRandomDate();

    QList<ComicItem> getComicsForTag(const QString& tag);
    QList<ComicItem> getComicsForDate(const QString& date);
    QList<ComicItem> getComicsForTranscript(const QString& term);

    QStringList getAllComicTags();
    // QStringList getImagePathsForTag(const QString& tag);
    // QStringList getImagePathsForTranscript(const QString& searchTerm);
    // QStringList getImagePathsForDate(const QString& dateStr);
    QStringList getTagsForCurrentComic();

    void resizeEvent(QResizeEvent* event) override;
    void performSearch(const QString& query, SearchMode mode);

    void loadAndDisplay(const QDate& date);
    void showRandom();
    void showPrevious();
    void showNext();

    void updateTagButtons();
};

#pragma once
#include <QDate>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QWidget>

class ComicViewerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ComicViewerWidget(QWidget* parent = nullptr);

    void showComic(const QDate& date, const QPixmap& pixmap);
    void setTags(const QStringList& tags);

signals:
    void previousRequested();
    void nextRequested();
    void randomRequested();
    void tagSelected(const QString& tag);
    void tagEdited(const QString& oldTag, const QString& newTag);

protected:
    void resizeEvent(QResizeEvent*) override;

private:
    void editTags(const QStringList& tags);

    QLabel* title;
    QLabel* image;
    QHBoxLayout* tagLayout;
    QPixmap current;
};

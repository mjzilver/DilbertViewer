#pragma once
#include <QComboBox>
#include <QDate>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QQueue>
#include <QTimer>
#include <QWidget>

#include "ComicItem.h"

class ComicSearchWidget : public QWidget {
    Q_OBJECT
public:
    explicit ComicSearchWidget(const QStringList& tags, QWidget* parent = nullptr);

    enum Mode { Tag, Date, Transcript };

    void showResults(const QList<ComicItem>& comics);
    void setInput(const QString& str);

signals:
    void searchRequested(const QString& query, Mode mode);
    void comicSelected(const QDate& date);

private slots:
    void onReturnPressed();
    void onItemClicked(QListWidgetItem*);
    void loadNextThumbnail();

private:
    QComboBox* modeBox;
    QLineEdit* edit;
    QListWidget* gallery;

    QQueue<ComicItem> pending;
    QTimer thumbTimer;
};

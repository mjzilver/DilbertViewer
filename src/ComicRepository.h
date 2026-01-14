#pragma once
#include <QSqlDatabase>
#include <QStringList>

#include "ComicItem.h"

class ComicRepository {
public:
    explicit ComicRepository(const QString& dbPath);
    ~ComicRepository();

    QStringList allTags() const;
    QStringList tagsForComic(const QDate& date) const;

    QList<ComicItem> comicsForTag(const QString& tag) const;
    QList<ComicItem> comicsForDate(const QString& date) const;
    QList<ComicItem> comicsForTranscript(const QString& text) const;

    void removeTagFromComic(const QDate& date, const QString& tagName);
    void addTagToComic(const QDate& date, const QString& tagName);

    void editTag(const QString& oldTag, const QString& newTag);

private:
    QSqlDatabase db;
};

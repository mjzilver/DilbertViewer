#include "ComicRepository.h"

#include <QDebug>
#include <QtSql>

ComicRepository::ComicRepository(const QString& dbPath) {
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);

    if (!db.open()) qFatal("Failed to open database");
}

ComicRepository::~ComicRepository() {
    if (db.isOpen()) db.close();
}

QStringList ComicRepository::allTags() const {
    QStringList tags;
    QSqlQuery q("SELECT name FROM tags ORDER BY name ASC", db);

    while (q.next()) tags << q.value(0).toString();

    return tags;
}

QStringList ComicRepository::tagsForComic(const QDate& date) const {
    QStringList tags;
    QSqlQuery q(db);

    q.prepare(
        "SELECT tags.name "
        "FROM tags "
        "JOIN comic_tags ON comic_tags.tag_id = tags.id "
        "WHERE comic_tags.comic_date = :date "
        "ORDER BY tags.name");

    q.bindValue(":date", date.toString(Qt::ISODate));
    q.exec();

    while (q.next()) tags << q.value(0).toString();

    return tags;
}

QList<ComicItem> ComicRepository::comicsForTag(const QString& tag) const {
    QList<ComicItem> out;
    QSqlQuery q(db);

    q.prepare(
        "SELECT comics.date, comics.image_path "
        "FROM comics "
        "JOIN comic_tags ON comic_tags.comic_date = comics.date "
        "JOIN tags ON tags.id = comic_tags.tag_id "
        "WHERE tags.name = :tag "
        "ORDER BY comics.date");

    q.bindValue(":tag", tag);
    q.exec();

    while (q.next())
        out.append({QDate::fromString(q.value(0).toString(), Qt::ISODate), q.value(1).toString()});

    return out;
}

QList<ComicItem> ComicRepository::comicsForDate(const QString& date) const {
    QList<ComicItem> out;
    QSqlQuery q(db);

    q.prepare(
        "SELECT date, image_path "
        "FROM comics "
        "WHERE date = :date");
    q.bindValue(":date", date);
    q.exec();

    while (q.next())
        out.append({QDate::fromString(q.value(0).toString(), Qt::ISODate), q.value(1).toString()});

    return out;
}

QList<ComicItem> ComicRepository::comicsForTranscript(const QString& text) const {
    QList<ComicItem> out;
    QSqlQuery q(db);

    q.prepare(
        "SELECT date, image_path "
        "FROM comics "
        "WHERE transcript LIKE :text "
        "ORDER BY date");
    q.bindValue(":text", "%" + text + "%");
    q.exec();

    while (q.next())
        out.append({QDate::fromString(q.value(0).toString(), Qt::ISODate), q.value(1).toString()});

    return out;
}

void ComicRepository::editTag(const QString& oldTag, const QString& newTag) {
    if (oldTag == newTag) return;

    QSqlQuery q(db);
    q.prepare("SELECT id FROM tags WHERE name = :new");
    q.bindValue(":new", newTag);

    if (!q.exec()) {
        qDebug() << "Failed to query new tag:" << q.lastError().text();
        return;
    }

    if (q.next()) {
        int newId = q.value(0).toInt();

        QSqlQuery oldIdQuery(db);
        oldIdQuery.prepare("SELECT id FROM tags WHERE name = :old");
        oldIdQuery.bindValue(":old", oldTag);
        if (!oldIdQuery.exec() || !oldIdQuery.next()) {
            qDebug() << "Old tag not found:" << oldTag;
            return;
        }
        int oldId = oldIdQuery.value(0).toInt();

        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE comic_tags SET tag_id = :newId WHERE tag_id = :oldId");
        updateQuery.bindValue(":newId", newId);
        updateQuery.bindValue(":oldId", oldId);
        if (!updateQuery.exec()) {
            qDebug() << "Failed to update comic_tags:" << updateQuery.lastError().text();
            return;
        }

        QSqlQuery deleteQuery(db);
        deleteQuery.prepare("DELETE FROM tags WHERE id = :oldId");
        deleteQuery.bindValue(":oldId", oldId);
        if (!deleteQuery.exec()) {
            qDebug() << "Failed to delete old tag:" << deleteQuery.lastError().text();
        }

    } else {
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE tags SET name = :new WHERE name = :old");
        updateQuery.bindValue(":new", newTag);
        updateQuery.bindValue(":old", oldTag);
        if (!updateQuery.exec()) {
            qDebug() << "Failed to update tag name:" << updateQuery.lastError().text();
        }
    }
}

void ComicRepository::addTagToComic(const QDate& date, const QString& tagName) {
    QSqlQuery q(db);

    q.prepare("SELECT id FROM tags WHERE name = :name");
    q.bindValue(":name", tagName);
    q.exec();

    int tagId;
    if (q.next()) {
        tagId = q.value(0).toInt();
    } else {
        QSqlQuery insertTag(db);
        insertTag.prepare("INSERT INTO tags(name) VALUES(:name)");
        insertTag.bindValue(":name", tagName);
        if (!insertTag.exec()) return;
        tagId = insertTag.lastInsertId().toInt();
    }

    QSqlQuery link(db);
    link.prepare("INSERT OR IGNORE INTO comic_tags(comic_date, tag_id) VALUES(:date, :tagId)");
    link.bindValue(":date", date.toString(Qt::ISODate));
    link.bindValue(":tagId", tagId);
    link.exec();
}

void ComicRepository::removeTagFromComic(const QDate& date, const QString& tagName) {
    QSqlQuery q(db);
    q.prepare("SELECT id FROM tags WHERE name = :name");
    q.bindValue(":name", tagName);
    if (!q.exec() || !q.next()) return;

    int tagId = q.value(0).toInt();

    QSqlQuery del(db);
    del.prepare("DELETE FROM comic_tags WHERE comic_date = :date AND tag_id = :tagId");
    del.bindValue(":date", date.toString(Qt::ISODate));
    del.bindValue(":tagId", tagId);
    del.exec();

    QSqlQuery check(db);
    check.prepare("SELECT COUNT(*) FROM comic_tags WHERE tag_id = :tagId");
    check.bindValue(":tagId", tagId);
    check.exec();
    if (check.next() && check.value(0).toInt() == 0) {
        QSqlQuery deleteTag(db);
        deleteTag.prepare("DELETE FROM tags WHERE id = :tagId");
        deleteTag.bindValue(":tagId", tagId);
        deleteTag.exec();
    }
}

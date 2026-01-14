#pragma once

#include <QObject>
#include <QDialog>
#include <QPushButton>
#include <QStringList>
#include <QWidget>

class ComicTagsEditorDialog : public QDialog {
    Q_OBJECT
public:
    ComicTagsEditorDialog(const QStringList& tags, QWidget* parent = nullptr);

signals:
    void tagEdited(const QString& oldTag, const QString& newTag);
    void tagRemoved(const QString& tag);
    void tagAdded(const QString& tag);

private:
    QStringList currentTags;
};

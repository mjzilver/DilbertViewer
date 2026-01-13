#pragma once

#include <QPushButton>
#include <QStringList>
#include <QWidget>
#include <functional>

#include "FlowLayout.h"

class ComicTagsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ComicTagsWidget(QWidget* parent = nullptr);

    void setTags(const QStringList& newTags);

signals:
    void tagSelected(const QString& tag);
    void tagEdited(const QString& oldTag, const QString& newTag);

public slots:
    void openEditDialog();

private:
    FlowLayout* layout;
    QStringList tags;

    QWidget* createLabel(const QString& text, const QColor& color = Qt::black);
    QPushButton* createButton(const QString& text, bool flat, std::function<void()> onClick);
};

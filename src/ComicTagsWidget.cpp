#include "ComicTagsWidget.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "ComicTagsEditorDialog.h"

ComicTagsWidget::ComicTagsWidget(QWidget* parent)
    : QWidget(parent), layout(new FlowLayout(this, 0, 6, 6)) {
    setLayout(layout);
}

void ComicTagsWidget::setTags(const QStringList& newTags) {
    tags = newTags;

    QLayoutItem* item;
    while ((item = layout->takeAt(0))) {
        if (item->widget()) delete item->widget();
        delete item;
    }

    if (tags.isEmpty()) {
        layout->addWidget(createLabel("No tags found", Qt::gray));
        return;
    }

    for (const QString& tag : tags) {
        layout->addWidget(createButton(tag, true, [this, tag] { emit tagSelected(tag); }));
    }

    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(spacer);

    if (editor && editor->isVisible()) {
        editor->close();
        editor->deleteLater();
        editor = nullptr;

        openEditDialog();
    }
}

QWidget* ComicTagsWidget::createLabel(const QString& text, const QColor& color) {
    auto* label = new QLabel(text);
    label->setStyleSheet(QString("color: %1").arg(color.name()));
    return label;
}

QPushButton* ComicTagsWidget::createButton(const QString& text, bool flat,
                                           const std::function<void()>& onClick) {
    auto* btn = new QPushButton(text);
    btn->setFlat(flat);
    connect(btn, &QPushButton::clicked, this, onClick);
    return btn;
}

void ComicTagsWidget::openEditDialog() {
    editor = new ComicTagsEditorDialog(tags, this);

    connect(editor, &ComicTagsEditorDialog::tagEdited, this, &ComicTagsWidget::tagEdited);
    connect(editor, &ComicTagsEditorDialog::tagRemoved, this, &ComicTagsWidget::tagRemoved);
    connect(editor, &ComicTagsEditorDialog::tagAdded, this, &ComicTagsWidget::tagAdded);

    editor->show();
}
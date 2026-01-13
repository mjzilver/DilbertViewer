#include "ComicTagsWidget.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

ComicTagsWidget::ComicTagsWidget(QWidget* parent) : QWidget(parent) {
    layout = new FlowLayout(this, 0, 6, 6);
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

    // viewer->addButton(createButton("Edit tags", false, [this, tags] { openEditDialog(tags); }));
}

QWidget* ComicTagsWidget::createLabel(const QString& text, const QColor& color) {
    auto* label = new QLabel(text);
    label->setStyleSheet(QString("color: %1").arg(color.name()));
    return label;
}

QPushButton* ComicTagsWidget::createButton(const QString& text, bool flat,
                                           std::function<void()> onClick) {
    auto* btn = new QPushButton(text);
    btn->setFlat(flat);
    connect(btn, &QPushButton::clicked, this, onClick);
    return btn;
}

void ComicTagsWidget::openEditDialog() {
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle("Edit Tags");
    QVBoxLayout* layout = new QVBoxLayout(dialog);

    for (const QString& tag : tags) {
        QHBoxLayout* rowLayout = new QHBoxLayout;
        QLineEdit* lineEdit = new QLineEdit(tag);
        QPushButton* saveBtn = new QPushButton("Save");

        rowLayout->addWidget(lineEdit);
        rowLayout->addWidget(saveBtn);

        connect(saveBtn, &QPushButton::clicked, this,
                [this, lineEdit, tag]() { emit tagEdited(tag, lineEdit->text()); });

        layout->addLayout(rowLayout);
    }

    QPushButton* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addWidget(closeBtn);

    dialog->setLayout(layout);
    dialog->setModal(false);
    dialog->show();
}

#include "ComicTagsEditorDialog.h"

#include <QLineEdit>
#include <QVBoxLayout>

ComicTagsEditorDialog::ComicTagsEditorDialog(const QStringList& tags, QWidget* parent)
    : QDialog(parent), currentTags(tags) {
    setWindowTitle("Edit Tags");
    QVBoxLayout* layout = new QVBoxLayout(this);

    for (const QString& tag : currentTags) {
        QHBoxLayout* rowLayout = new QHBoxLayout;
        QLineEdit* lineEdit = new QLineEdit(tag);
        QPushButton* saveBtn = new QPushButton("Save");
        QPushButton* removeBtn = new QPushButton("Remove");

        rowLayout->addWidget(lineEdit);
        rowLayout->addWidget(saveBtn);
        rowLayout->addWidget(removeBtn);

        connect(saveBtn, &QPushButton::clicked, this,
                [this, tag, lineEdit]() { emit tagEdited(tag, lineEdit->text()); });

        connect(removeBtn, &QPushButton::clicked, this, [this, tag]() { emit tagRemoved(tag); });

        layout->addLayout(rowLayout);
    }

    QPushButton* addBtn = new QPushButton("Add tag");
    layout->addWidget(addBtn);

    connect(addBtn, &QPushButton::clicked, this, [this]() {
        QDialog addDialog(this);
        addDialog.setWindowTitle("Add Tag");

        QVBoxLayout* addLayout = new QVBoxLayout(&addDialog);
        QLineEdit* addLineEdit = new QLineEdit;
        addLineEdit->setPlaceholderText("New tag");
        QPushButton* addTagBtn = new QPushButton("Add");
        QPushButton* cancelBtn = new QPushButton("Cancel");

        addLayout->addWidget(addLineEdit);
        addLayout->addWidget(addTagBtn);
        addLayout->addWidget(cancelBtn);

        connect(addTagBtn, &QPushButton::clicked, &addDialog, [this, addLineEdit, &addDialog]() {
            QString newTag = addLineEdit->text().trimmed();
            if (!newTag.isEmpty()) emit tagAdded(newTag);
            addDialog.accept();
        });

        connect(cancelBtn, &QPushButton::clicked, &addDialog, &QDialog::reject);

        addDialog.exec();
    });

    QPushButton* closeBtn = new QPushButton("Close");
    layout->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    setLayout(layout);
}

#include "ComicTagsWidget.h"

#include <QDialog>
#include <QLineEdit>

ComicTagsWidget::ComicTagsWidget(QWidget* parent) : QWidget(parent) {
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(6);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    createNewRow();
}

void ComicTagsWidget::setTags(const QStringList& tags) {
    clearWidgets();
    if (tags.isEmpty()) {
        addWidget(createLabel("No tags found", Qt::gray));
        return;
    }

    for (const QString& tag : tags) {
        addWidget(createButton(tag, true, [this, tag] { emit tagSelected(tag); }));
    }

    addWidget(createButton("Edit tags", false, [this, tags] { openEditDialog(tags); }));
}

int ComicTagsWidget::widgetTextWidth(QWidget* w) const {
    int textWidth = 0;

    if (auto* label = qobject_cast<QLabel*>(w)) {
        QFontMetrics fm(label->font());
        textWidth = fm.horizontalAdvance(label->text());
        textWidth += label->contentsMargins().left() + label->contentsMargins().right() + 10;
    } else if (auto* button = qobject_cast<QPushButton*>(w)) {
        QFontMetrics fm(button->font());
        textWidth = fm.horizontalAdvance(button->text());
        textWidth += button->contentsMargins().left() + button->contentsMargins().right() + 20;
    } else {
        textWidth = w->sizeHint().width();
    }

    return textWidth;
}

void ComicTagsWidget::createNewRow() {
    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    row->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(row);
    rows.append(row);
}

void ComicTagsWidget::addWidget(QWidget* w) {
    if (!w) return;
    w->setParent(this);
    w->adjustSize();
    allWidgets.append(w);
    placeWidget(w);
}

void ComicTagsWidget::clearWidgets() {
    qDeleteAll(allWidgets);
    allWidgets.clear();
    for (auto* row : rows) delete row;
    rows.clear();
    createNewRow();
}

QLabel* ComicTagsWidget::createLabel(const QString& text, const QColor& color) {
    auto* label = new QLabel(text);
    label->setStyleSheet(QString("color: %1").arg(color.name()));
    return label;
}

QPushButton* ComicTagsWidget::createButton(const QString& text, bool flat,
                                           std::function<void()> onClick) {
    auto* btn = new QPushButton(text);
    btn->setFlat(flat);
    btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QFontMetrics fm(btn->font());
    int minWidth = fm.horizontalAdvance(text) + 20;
    btn->setMinimumWidth(minWidth);
    connect(btn, &QPushButton::clicked, this, onClick);
    return btn;
}

void ComicTagsWidget::placeWidget(QWidget* w) {
    int wWidth = widgetTextWidth(w);
    int maxWidth = parentWidget() ? parentWidget()->width() : contentsRect().width();
    QHBoxLayout* row = rows.last();

    if (rowWidth(row) + wWidth > maxWidth && row->count() > 0) {
        createNewRow();
        row = rows.last();
    }

    row->addWidget(w);
}

int ComicTagsWidget::rowWidth(QHBoxLayout* row) const {
    int width = 0;
    for (int i = 0; i < row->count(); ++i) {
        if (auto* w = row->itemAt(i)->widget()) {
            width += widgetTextWidth(w);
        }
    }
    return width;
}

void ComicTagsWidget::repositionWidgets() {
    for (auto* row : rows) {
        mainLayout->removeItem(row);
        delete row;
    }
    rows.clear();
    createNewRow();

    int maxWidth = parentWidget() ? parentWidget()->width() : contentsRect().width();
    int currentWidth = 0;
    QHBoxLayout* row = rows.last();

    for (auto* w : allWidgets) {
        int wWidth = widgetTextWidth(w);
        if (currentWidth + wWidth > maxWidth && row->count() > 0) {
            createNewRow();
            row = rows.last();
            currentWidth = 0;
        }
        row->addWidget(w);
        currentWidth += wWidth;
    }
}

void ComicTagsWidget::openEditDialog(const QStringList& tags) {
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

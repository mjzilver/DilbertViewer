#pragma once
#include <QDialog>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <cmath>

class ComicTagsWidget : public QWidget {
    Q_OBJECT

public:
    explicit ComicTagsWidget(QWidget* parent = nullptr);

    void setTags(const QStringList& tags);

signals:
    void tagSelected(const QString& tag);
    void tagEdited(const QString& oldTag, const QString& newTag);

protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        repositionWidgets();
    }

private:
    QVBoxLayout* mainLayout;
    QList<QWidget*> allWidgets;
    QList<QHBoxLayout*> rows;

    int widgetTextWidth(QWidget* w) const;

    void createNewRow();

    void addWidget(QWidget* w);

    void clearWidgets();

    QLabel* createLabel(const QString& text, const QColor& color = Qt::black);

    QPushButton* createButton(const QString& text, bool flat, std::function<void()> onClick);

    void placeWidget(QWidget* w);

    int rowWidth(QHBoxLayout* row) const;

    void repositionWidgets();

    void openEditDialog(const QStringList& tags);
};

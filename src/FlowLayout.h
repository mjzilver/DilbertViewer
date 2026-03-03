#pragma once

#include <QLayout>
#include <QRect>
#include <QStyle>

class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = 0, int hSpacing = 0,
                        int vSpacing = 0);

    ~FlowLayout() override;

    void addItem(QLayoutItem* item) override;
    int count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;

    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;

    void setGeometry(const QRect& rect) override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;

private:
    struct Row {
        QVector<QLayoutItem*> items;
        int width = 0;
        int height = 0;
    };

    int horizontalSpacing() const;
    int verticalSpacing() const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QRect adjustedArea(const QRect& rect) const;

    QVector<Row> buildRows(int maxWidth) const;
    int calculateTotalHeight(const QVector<Row>& rows) const;
    void applyLayout(const QRect& area, const QVector<Row>& rows);

private:
    QVector<QLayoutItem*> itemList;
    int hSpace;
    int vSpace;
};

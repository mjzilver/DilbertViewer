#include "FlowLayout.h"

#include <QtWidgets>

FlowLayout::FlowLayout(QWidget* parent, int margin, int hSpacing, int vSpacing)
    : QLayout(parent), hSpace(hSpacing), vSpace(vSpacing) {
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout() {
    while (QLayoutItem* item = takeAt(0)) delete item;
}

void FlowLayout::addItem(QLayoutItem* item) { itemList.append(item); }

int FlowLayout::count() const { return itemList.size(); }

QLayoutItem* FlowLayout::itemAt(int index) const { return itemList.value(index); }

QLayoutItem* FlowLayout::takeAt(int index) {
    if (index >= 0 && index < itemList.size()) return itemList.takeAt(index);
    return nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const { return {}; }

bool FlowLayout::hasHeightForWidth() const { return true; }

int FlowLayout::heightForWidth(int width) const {
    const QRect area = adjustedArea(QRect(0, 0, width, 0));
    const auto rows = buildRows(area.width());
    return calculateTotalHeight(rows);
}

void FlowLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);

    const QRect area = adjustedArea(rect);
    const auto rows = buildRows(area.width());
    applyLayout(area, rows);
}

QSize FlowLayout::sizeHint() const { return minimumSize(); }

QSize FlowLayout::minimumSize() const {
    const QRect area = adjustedArea(QRect(0, 0, contentsRect().width(), 0));
    const auto rows = buildRows(area.width());
    return QSize(0, calculateTotalHeight(rows));
}

QRect FlowLayout::adjustedArea(const QRect& rect) const {
    const QMargins margins = contentsMargins();
    return rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
}

int FlowLayout::horizontalSpacing() const {
    return (hSpace >= 0) ? hSpace : smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
}

int FlowLayout::verticalSpacing() const {
    return (vSpace >= 0) ? vSpace : smartSpacing(QStyle::PM_LayoutVerticalSpacing);
}

QVector<FlowLayout::Row> FlowLayout::buildRows(int maxWidth) const {
    QVector<Row> rows;
    if (itemList.isEmpty()) return rows;

    const int spaceX = horizontalSpacing();

    Row currentRow;

    for (auto* item : itemList) {
        const QSize hint = item->sizeHint();

        const int nextWidth =
            currentRow.items.isEmpty() ? hint.width() : currentRow.width + spaceX + hint.width();

        if (!currentRow.items.isEmpty() && nextWidth > maxWidth) {
            rows.append(currentRow);
            currentRow = Row{};
        }

        if (!currentRow.items.isEmpty()) currentRow.width += spaceX;

        currentRow.items.append(item);
        currentRow.width += hint.width();
        currentRow.height = std::max(currentRow.height, hint.height());
    }

    if (!currentRow.items.isEmpty()) rows.append(currentRow);

    return rows;
}

int FlowLayout::calculateTotalHeight(const QVector<Row>& rows) const {
    if (rows.isEmpty()) return 0;

    const int spaceY = verticalSpacing();

    int totalHeight = 0;
    for (const Row& row : rows) totalHeight += row.height;

    totalHeight += spaceY * (rows.size() - 1);

    const QMargins margins = contentsMargins();
    return totalHeight + margins.top() + margins.bottom();
}

void FlowLayout::applyLayout(const QRect& area, const QVector<Row>& rows) {
    const int spaceX = horizontalSpacing();
    const int spaceY = verticalSpacing();

    const int totalHeight =
        calculateTotalHeight(rows) - contentsMargins().top() - contentsMargins().bottom();

    int y = area.y() + (area.height() - totalHeight) / 2;

    for (const Row& row : rows) {
        int x = area.x() + (area.width() - row.width) / 2;

        for (auto* item : row.items) {
            const QSize hint = item->sizeHint();
            item->setGeometry(QRect(QPoint(x, y), hint));
            x += hint.width() + spaceX;
        }

        y += row.height + spaceY;
    }
}

int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const {
    QObject* parentObj = parent();

    if (!parentObj) return -1;

    if (parentObj->isWidgetType()) {
        QWidget* parentWidget = static_cast<QWidget*>(parentObj);
        return parentWidget->style()->pixelMetric(pm, nullptr, parentWidget);
    }

    return static_cast<QLayout*>(parentObj)->spacing();
}
#pragma once

#include <QString>
#include <QList>
#include <QPair>
#include <QPainter>
#include <QPrinter>
#include <QTableView>
#include "SpreadsheetModel.h"

struct PrintOptions {
    enum RangeMode { UsedRange, SelectedRange, CustomRange };
    RangeMode rangeMode = UsedRange;
    QString customStartCell = "A1";
    QString customEndCell = "";
    bool printGridlines = true;
    bool printHeaders = false;
    bool fitToPageWidth = true;
    bool centerHorizontal = true;
    bool centerVertical = false;
    int offsetX = 0;
    int offsetY = 0;
};

class SpreadsheetPrinter {
public:
    static void print(QPrinter *printer, SpreadsheetModel *model, QTableView *view, const PrintOptions &options);
    static bool parseCellRef(const QString &ref, int &row, int &col);
    static QString toCellRef(int row, int col);
    static void getTargetRange(SpreadsheetModel *model, QTableView *view, const PrintOptions &options, int &minR, int &minC, int &maxR, int &maxC);

private:
    static void renderPage(QPainter &painter, SpreadsheetModel *model, QTableView *view, const PrintOptions &options,
                           const QList<int> &rows, const QList<int> &cols, qreal scale, qreal colHeaderHeight, qreal rowHeaderWidth, qreal pageW, qreal pageH);
};

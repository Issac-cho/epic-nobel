#include "SpreadsheetPrinter.h"
#include <QItemSelectionModel>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QVariant>
#include <QPageLayout>
#include <QtMath>

bool SpreadsheetPrinter::parseCellRef(const QString &ref, int &row, int &col) {
    QString trimmed = ref.trimmed().toUpper();
    if (trimmed.isEmpty()) return false;
    int i = 0;
    while (i < trimmed.length() && trimmed[i].isLetter()) {
        i++;
    }
    if (i == 0 || i == trimmed.length()) return false;
    QString colPart = trimmed.left(i);
    QString rowPart = trimmed.mid(i);
    bool ok;
    int r = rowPart.toInt(&ok) - 1;
    if (!ok || r < 0) return false;
    
    int c = 0;
    for (int j = 0; j < colPart.length(); ++j) {
        c = c * 26 + (colPart[j].unicode() - 'A' + 1);
    }
    col = c - 1;
    row = r;
    return true;
}

QString SpreadsheetPrinter::toCellRef(int row, int col) {
    QString colStr;
    int c = col + 1;
    while (c > 0) {
        int rem = (c - 1) % 26;
        colStr.prepend(QChar('A' + rem));
        c = (c - 1) / 26;
    }
    return colStr + QString::number(row + 1);
}

void SpreadsheetPrinter::getTargetRange(SpreadsheetModel *model, QTableView *view, const PrintOptions &options, int &minR, int &minC, int &maxR, int &maxC) {
    if (!model) return;
    minR = 0; minC = 0; maxR = 0; maxC = 0;

    if (options.rangeMode == PrintOptions::CustomRange) {
        int r1, c1, r2, c2;
        bool ok1 = parseCellRef(options.customStartCell, r1, c1);
        bool ok2 = parseCellRef(options.customEndCell, r2, c2);
        if (ok1 && ok2) {
            minR = qBound(0, qMin(r1, r2), model->rowCount() - 1);
            maxR = qBound(0, qMax(r1, r2), model->rowCount() - 1);
            minC = qBound(0, qMin(c1, c2), model->columnCount() - 1);
            maxC = qBound(0, qMax(c1, c2), model->columnCount() - 1);
            return;
        }
    } else if (options.rangeMode == PrintOptions::SelectedRange && view && view->selectionModel()->hasSelection()) {
        QModelIndexList sel = view->selectionModel()->selectedIndexes();
        if (!sel.isEmpty()) {
            minR = sel.first().row(); maxR = minR;
            minC = sel.first().column(); maxC = minC;
            for (const QModelIndex &idx : sel) {
                if (idx.row() < minR) minR = idx.row();
                if (idx.row() > maxR) maxR = idx.row();
                if (idx.column() < minC) minC = idx.column();
                if (idx.column() > maxC) maxC = idx.column();
            }
            return;
        }
    }

    // Default: UsedRange
    model->getUsedRange(minR, minC, maxR, maxC);
}

void SpreadsheetPrinter::print(QPrinter *printer, SpreadsheetModel *model, QTableView *view, const PrintOptions &options) {
    if (!printer || !model) return;

    int minR = 0, minC = 0, maxR = 0, maxC = 0;
    getTargetRange(model, view, options, minR, minC, maxR, maxC);

    qreal colHeaderHeight = options.printHeaders ? 26.0 : 0.0;
    qreal rowHeaderWidth = options.printHeaders ? 45.0 : 0.0;

    qreal dpiScale = qreal(printer->resolution()) / 96.0;
    if (dpiScale <= 0.001) dpiScale = 1.0;
    QRectF pageRectDevice = printer->pageRect(QPrinter::DevicePixel);
    qreal effectivePageW = pageRectDevice.width() / dpiScale;
    qreal effectivePageH = pageRectDevice.height() / dpiScale;

    qreal tableW = rowHeaderWidth;
    for (int c = minC; c <= maxC; ++c) {
        tableW += (view ? view->columnWidth(c) : 100);
    }

    qreal fitScale = 1.0;
    if (options.fitToPageWidth && tableW > effectivePageW && tableW > 0.1) {
        fitScale = effectivePageW / tableW;
    }

    qreal totalScale = dpiScale * fitScale;
    qreal pageW = pageRectDevice.width() / totalScale;
    qreal pageH = pageRectDevice.height() / totalScale;

    // Paginate Horizontally
    QList<QList<int>> hSlices;
    QList<int> currHSlice;
    qreal currW = rowHeaderWidth;
    for (int c = minC; c <= maxC; ++c) {
        qreal w = (view ? view->columnWidth(c) : 100);
        if (!currHSlice.isEmpty() && currW + w > pageW) {
            hSlices.append(currHSlice);
            currHSlice.clear();
            currW = rowHeaderWidth;
        }
        currHSlice.append(c);
        currW += w;
    }
    if (!currHSlice.isEmpty()) hSlices.append(currHSlice);

    // Paginate Vertically
    QList<QList<int>> vSlices;
    QList<int> currVSlice;
    qreal currHeight = colHeaderHeight;
    for (int r = minR; r <= maxR; ++r) {
        qreal h = (view ? view->rowHeight(r) : 25);
        if (!currVSlice.isEmpty() && currHeight + h > pageH) {
            vSlices.append(currVSlice);
            currVSlice.clear();
            currHeight = colHeaderHeight;
        }
        currVSlice.append(r);
        currHeight += h;
    }
    if (!currVSlice.isEmpty()) vSlices.append(currVSlice);

    QPainter painter(printer);
    bool firstPage = true;
    for (int hs = 0; hs < hSlices.size(); ++hs) {
        for (int vs = 0; vs < vSlices.size(); ++vs) {
            if (!firstPage) {
                printer->newPage();
            }
            firstPage = false;
            renderPage(painter, model, view, options, vSlices[vs], hSlices[hs], totalScale, colHeaderHeight, rowHeaderWidth, pageW, pageH);
        }
    }
}

void SpreadsheetPrinter::renderPage(QPainter &painter, SpreadsheetModel *model, QTableView *view, const PrintOptions &options,
                                    const QList<int> &rows, const QList<int> &cols, qreal scale, qreal colHeaderHeight, qreal rowHeaderWidth, qreal pageW, qreal pageH) {
    if (rows.isEmpty() || cols.isEmpty()) return;

    painter.save();
    painter.scale(scale, scale);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    qreal sliceW = rowHeaderWidth;
    for (int c : cols) sliceW += (view ? view->columnWidth(c) : 100);
    qreal sliceH = colHeaderHeight;
    for (int r : rows) sliceH += (view ? view->rowHeight(r) : 25);

    qreal transX = options.offsetX;
    qreal transY = options.offsetY;
    if (options.centerHorizontal) transX += (pageW - sliceW) / 2.0;
    if (options.centerVertical) transY += (pageH - sliceH) / 2.0;

    painter.translate(transX, transY);

    QFont defaultFont = view ? view->font() : QFont("Malgun Gothic", 10);
    if (defaultFont.pointSize() > 0) {
        defaultFont.setPixelSize(qRound(defaultFont.pointSizeF() * 96.0 / 72.0));
    }
    QPen gridPen(QColor("#d0d0d0"), 1);
    QPen headerTextPen(QColor("#333333"));
    QBrush headerBg(QColor("#f4f5f7"));

    // Draw Column Headers
    if (options.printHeaders) {
        painter.setFont(defaultFont);
        qreal x = rowHeaderWidth;
        for (int c : cols) {
            qreal w = (view ? view->columnWidth(c) : 100);
            QRectF hRect(x, 0, w, colHeaderHeight);
            painter.fillRect(hRect, headerBg);
            painter.setPen(gridPen);
            painter.drawRect(hRect);
            painter.setPen(headerTextPen);
            QString colLetter = toCellRef(0, c);
            colLetter.chop(1);
            painter.drawText(hRect, Qt::AlignCenter, colLetter);
            x += w;
        }
        QRectF cornerRect(0, 0, rowHeaderWidth, colHeaderHeight);
        painter.fillRect(cornerRect, headerBg);
        painter.setPen(gridPen);
        painter.drawRect(cornerRect);
    }

    // Draw Rows and Cells
    qreal y = colHeaderHeight;
    for (int r : rows) {
        qreal h = (view ? view->rowHeight(r) : 25);
        if (options.printHeaders) {
            QRectF rRect(0, y, rowHeaderWidth, h);
            painter.fillRect(rRect, headerBg);
            painter.setPen(gridPen);
            painter.drawRect(rRect);
            painter.setPen(headerTextPen);
            painter.setFont(defaultFont);
            painter.drawText(rRect, Qt::AlignCenter, QString::number(r + 1));
        }

        qreal x = rowHeaderWidth;
        for (int c : cols) {
            qreal w = (view ? view->columnWidth(c) : 100);

            // Merge check
            bool isMerged = false;
            QRect mergeRect;
            for (const QRect &m : model->getMergedRanges()) {
                if (m.contains(c, r)) {
                    isMerged = true;
                    mergeRect = m;
                    break;
                }
            }

            if (isMerged) {
                int anchorR = qMax(mergeRect.y(), rows.first());
                int anchorC = qMax(mergeRect.x(), cols.first());
                if (r != anchorR || c != anchorC) {
                    x += w;
                    continue; // skip covered cell
                }
                int endR = qMin(mergeRect.bottom(), rows.last());
                int endC = qMin(mergeRect.right(), cols.last());
                qreal mergeW = 0, mergeH = 0;
                for (int mc = anchorC; mc <= endC; ++mc) mergeW += (view ? view->columnWidth(mc) : 100);
                for (int mr = anchorR; mr <= endR; ++mr) mergeH += (view ? view->rowHeight(mr) : 25);

                QRectF cellRect(x, y, mergeW, mergeH);
                QModelIndex origIdx = model->index(mergeRect.y(), mergeRect.x());
                QVariant bgVal = model->data(origIdx, Qt::BackgroundRole);
                if (bgVal.isValid() && bgVal.canConvert<QBrush>()) {
                    painter.fillRect(cellRect, qvariant_cast<QBrush>(bgVal));
                }
                if (options.printGridlines) {
                    painter.setPen(gridPen);
                    painter.drawRect(cellRect);
                }
                QString text = model->data(origIdx, Qt::DisplayRole).toString();
                if (!text.isEmpty()) {
                    QVariant fontVar = model->data(origIdx, Qt::FontRole);
                    QFont f = defaultFont;
                    if (fontVar.isValid()) {
                        QFont customFont = fontVar.value<QFont>();
                        if (!customFont.family().isEmpty()) f.setFamily(customFont.family());
                        f.setBold(customFont.bold());
                        f.setItalic(customFont.italic());
                        f.setUnderline(customFont.underline());
                        if (customFont.pointSize() > 0) {
                            f.setPixelSize(qRound(customFont.pointSizeF() * 96.0 / 72.0));
                        } else if (customFont.pixelSize() > 0) {
                            f.setPixelSize(customFont.pixelSize());
                        }
                    }
                    painter.setFont(f);
                    QColor fg = model->data(origIdx, Qt::ForegroundRole).value<QColor>();
                    painter.setPen(fg.isValid() ? fg : QColor("#000000"));
                    int align = model->data(origIdx, Qt::TextAlignmentRole).toInt();
                    if (align == 0) align = Qt::AlignLeft | Qt::AlignVCenter;
                    painter.save();
                    painter.setClipRect(cellRect);
                    painter.drawText(cellRect.adjusted(5, 2, -5, -2), align | Qt::TextSingleLine, text);
                    painter.restore();
                }
                x += w;
                continue;
            }

            // Normal cell
            QRectF cellRect(x, y, w, h);
            QModelIndex idx = model->index(r, c);
            QVariant bgVal = model->data(idx, Qt::BackgroundRole);
            if (bgVal.isValid() && bgVal.canConvert<QBrush>()) {
                painter.fillRect(cellRect, qvariant_cast<QBrush>(bgVal));
            }
            if (options.printGridlines) {
                painter.setPen(gridPen);
                painter.drawRect(cellRect);
            }
            QString text = model->data(idx, Qt::DisplayRole).toString();
            if (!text.isEmpty()) {
                QVariant fontVar = model->data(idx, Qt::FontRole);
                QFont f = defaultFont;
                if (fontVar.isValid()) {
                    QFont customFont = fontVar.value<QFont>();
                    if (!customFont.family().isEmpty()) f.setFamily(customFont.family());
                    f.setBold(customFont.bold());
                    f.setItalic(customFont.italic());
                    f.setUnderline(customFont.underline());
                    if (customFont.pointSize() > 0) {
                        f.setPixelSize(qRound(customFont.pointSizeF() * 96.0 / 72.0));
                    } else if (customFont.pixelSize() > 0) {
                        f.setPixelSize(customFont.pixelSize());
                    }
                }
                painter.setFont(f);
                QColor fg = model->data(idx, Qt::ForegroundRole).value<QColor>();
                painter.setPen(fg.isValid() ? fg : QColor("#000000"));
                int align = model->data(idx, Qt::TextAlignmentRole).toInt();
                if (align == 0) align = Qt::AlignLeft | Qt::AlignVCenter;
                painter.save();
                painter.setClipRect(cellRect);
                painter.drawText(cellRect.adjusted(5, 2, -5, -2), align | Qt::TextSingleLine, text);
                painter.restore();
            }

            x += w;
        }
        y += h;
    }
    // Draw custom borders
    y = colHeaderHeight;
    for (int r : rows) {
        qreal h = (view ? view->rowHeight(r) : 25);
        qreal x = rowHeaderWidth;
        for (int c : cols) {
            qreal w = (view ? view->columnWidth(c) : 100);

            bool isMerged = false;
            QRect mergeRect;
            for (const QRect &m : model->getMergedRanges()) {
                if (m.contains(c, r)) {
                    isMerged = true;
                    mergeRect = m;
                    break;
                }
            }

            QRectF cellRect;
            int borderFlags = 0;
            if (isMerged) {
                int anchorR = qMax(mergeRect.y(), rows.first());
                int anchorC = qMax(mergeRect.x(), cols.first());
                if (r != anchorR || c != anchorC) {
                    x += w;
                    continue;
                }
                int endR = qMin(mergeRect.bottom(), rows.last());
                int endC = qMin(mergeRect.right(), cols.last());
                qreal mergeW = 0, mergeH = 0;
                for (int mc = anchorC; mc <= endC; ++mc) mergeW += (view ? view->columnWidth(mc) : 100);
                for (int mr = anchorR; mr <= endR; ++mr) mergeH += (view ? view->rowHeight(mr) : 25);
                cellRect = QRectF(x, y, mergeW, mergeH);
                borderFlags = model->data(model->index(mergeRect.y(), mergeRect.x()), Qt::UserRole + 2).toInt();
            } else {
                cellRect = QRectF(x, y, w, h);
                borderFlags = model->data(model->index(r, c), Qt::UserRole + 2).toInt();
            }

            if (borderFlags > 0) {
                painter.save();
                int top = (borderFlags >> 0) & 0xF;
                int bottom = (borderFlags >> 4) & 0xF;
                int left = (borderFlags >> 8) & 0xF;
                int right = (borderFlags >> 12) & 0xF;

                auto drawEdge = [&](qreal x1, qreal y1, qreal x2, qreal y2, int style, bool isHoriz) {
                    if (style == 0) return;
                    QPen pen(Qt::black);
                    if (style == 3) {
                        pen.setWidth(1);
                        painter.setPen(pen);
                        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
                        if (isHoriz) {
                            qreal offset = (y1 == cellRect.top()) ? 2 : -2;
                            painter.drawLine(QPointF(x1, y1 + offset), QPointF(x2, y2 + offset));
                        } else {
                            qreal offset = (x1 == cellRect.left()) ? 2 : -2;
                            painter.drawLine(QPointF(x1 + offset, y1), QPointF(x2 + offset, y2));
                        }
                    } else {
                        pen.setWidth(style == 2 ? 3 : 1);
                        painter.setPen(pen);
                        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
                    }
                };

                if (top) drawEdge(cellRect.left(), cellRect.top(), cellRect.right(), cellRect.top(), top, true);
                if (bottom) drawEdge(cellRect.left(), cellRect.bottom(), cellRect.right(), cellRect.bottom(), bottom, true);
                if (left) drawEdge(cellRect.left(), cellRect.top(), cellRect.left(), cellRect.bottom(), left, false);
                if (right) drawEdge(cellRect.right(), cellRect.top(), cellRect.right(), cellRect.bottom(), right, false);
                painter.restore();
            }

            x += w;
        }
        y += h;
    }

    painter.restore();
}

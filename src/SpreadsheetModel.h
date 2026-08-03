#pragma once
#include <QAbstractTableModel>
#include <QFile>
#include <QStringList>
#include <vector>
#include <QHash>
#include <QPair>
#include <QFont>
#include <QColor>
#include <QBrush>
#include <QRect>
#include <QList>
#include <QJsonObject>
#include <QXmlStreamWriter>
#include <QSet>

inline size_t qHash(const QPair<int, int> &key, size_t seed = 0) {
    return qHash(key.first, seed) ^ qHash(key.second, seed);
}

class SpreadsheetModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit SpreadsheetModel(QObject *parent = nullptr);
    ~SpreadsheetModel();
    
    bool loadCsv(const QString &filePath);
    bool saveCsv(const QString &filePath);
    bool saveCx(const QString &filePath);
    bool loadCx(const QString &filePath);
    bool exportToExcel(const QString &filePath);
    
    QJsonObject toJsonObject() const;
    bool fromJsonObject(const QJsonObject &root);
    void copyFrom(const SpreadsheetModel *src);
    void collectStyles(QXmlStreamWriter &xml, QHash<QString, QString> &styleToId, int &styleIdx) const;
    void exportWorksheetToXml(QXmlStreamWriter &xml, const QString &sheetName, QHash<QString, QString> &styleToId, int &styleIdx);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool insertColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;

    // Style
    void setCellBold(const QModelIndexList &indexes);
    void setCellItalic(const QModelIndexList &indexes);
    void setFontSize(const QModelIndexList &indexes, int size);
    void setCellBackgroundColor(const QModelIndexList &indexes, const QColor &color);
    void setCellTextColor(const QModelIndexList &indexes, const QColor &color);
    void setCellAlignment(const QModelIndexList &indexes, int alignment);
    void applyVerticalText(const QModelIndexList &indexes, bool enable);
    void applyFormat(const QModelIndexList &indexes, const QString &format);

    // Merge
    void addMergedRange(const QRect &range);
    void removeMergedRange(const QRect &range);
    const QList<QRect>& getMergedRanges() const { return m_mergedRanges; }
    bool isCoveredByMerge(int logicalRow, int col, QPair<int, int> *topLeft = nullptr) const;

    // Borders
    enum BorderEdge { BorderNone = 0, BorderThin = 1, BorderThick = 2, BorderDouble = 3 };
    void applyBorders(const QModelIndexList &indexes, const QString &type);

    void updateMergedRangesOnInsertRows(int row, int count);
    void updateMergedRangesOnRemoveRows(int row, int count);
    void updateMergedRangesOnInsertCols(int col, int count);
    void updateMergedRangesOnRemoveCols(int col, int count);

    // Filter & Sort
    void setFilterRange(int startRow, int startCol, int endCol);
    bool isFilterHeader(int row, int col) const;
    void sortRange(int column, Qt::SortOrder order);
    void clearFilter();
    bool hasFilter() const { return m_filterHeaderRow != -1; }

    // Undo / Redo
    void pushUndo();
    void undo();
    void redo();
    void beginMacro();
    void endMacro();
    void clearUndoStack();

    void setRowHeight(int row, int height) { m_rowHeights[row] = height; }
    void setColWidth(int col, int width) { m_colWidths[col] = width; }
    QHash<int, int> getRowHeights() const { return m_rowHeights; }
    QHash<int, int> getColWidths() const { return m_colWidths; }
    QHash<QPair<int, int>, int> getBorders() const { return m_borders; }

signals:
    void sizesRestored();

public:
    void setCursorPos(int r, int c) { m_cursorPos = qMakePair(r, c); }
    QPair<int, int> getCursorPos() const { return m_cursorPos; }
    void getUsedRange(int &minR, int &minC, int &maxR, int &maxC) const;

private:
    QString getCellData(int logicalRow, int col) const;
    QString getRawCellValue(int visualRow, int col) const;
    QString evaluateFormula(const QString &formula) const;
    int colFromLetters(const QString &letters) const;
    QSet<QPair<int, int>> expandToMergedPositions(const QModelIndexList &indexes) const;

    QFile *m_file;
    std::vector<qint64> m_lineOffsets;
    int m_columnCount;

    std::vector<int> m_rowMap; // Visual Row -> Logical Row Mapping (전체 정렬)
    std::vector<int> m_rangeRowMap; // 부분 범위 필터용 매핑 (요구사항 2번)
    int m_filterHeaderRow;
    int m_filterStartCol;
    int m_filterEndCol;

    QHash<QPair<int, int>, QString> m_editedData;
    QHash<QPair<int, int>, QFont> m_fonts;
    QHash<QPair<int, int>, QColor> m_bgColors;
    QHash<QPair<int, int>, QColor> m_fgColors;
    QHash<QPair<int, int>, int> m_alignments;
    QHash<QPair<int, int>, int> m_borders; // Format: (top << 0) | (bottom << 4) | (left << 8) | (right << 12)
    QHash<QPair<int, int>, bool> m_verticalTexts;
    QHash<QPair<int, int>, QString> m_formats; // General, Accounting, etc.
    QList<QRect> m_mergedRanges;
    
    QHash<int, int> m_rowHeights;
    QHash<int, int> m_colWidths;

    QList<QJsonObject> m_undoStack;
    QList<QJsonObject> m_redoStack;
    int m_macroDepth = 0;
    QPair<int, int> m_cursorPos = qMakePair(0, 0);
};

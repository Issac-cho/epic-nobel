#pragma once
#include <QTableView>
#include <QKeyEvent>
#include <QStyledItemDelegate>

class CustomTableView : public QTableView {
    Q_OBJECT
public:
    explicit CustomTableView(QWidget *parent = nullptr);
    void commitCurrentEditor();
    void restoreMergedRanges();

protected:
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QModelIndex findCtrlArrowTarget(const QModelIndex &start, int dr, int dc);
    int m_tabStartCol;
    QModelIndex m_selectionAnchor;
};

class SpreadsheetDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit SpreadsheetDelegate(QObject *parent = nullptr);
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool eventFilter(QObject *object, QEvent *event) override;

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
};

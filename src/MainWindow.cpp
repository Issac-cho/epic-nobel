#include "MainWindow.h"
#include "SpreadsheetModel.h"
#include <QVBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QInputDialog>
#include <QLabel>
#include <QColorDialog>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QSet>
#include <QToolButton>
#include <QTabBar>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QIcon>
#include <QTimer>
#include "SpreadsheetPrinter.h"
#include <QPrintPreviewWidget>
#include <QPrintDialog>
#include <QPrinter>
#include <QSpinBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QPlainTextEdit>

QWidget* MainWindow::s_activeFormulaEditor = nullptr;
QString MainWindow::s_formulaStartSheet = "";
QString MainWindow::s_formulaTextBeforeDrag = "";
bool MainWindow::s_isSelectingFormulaRange = false;
#include <QDialog>
#include <QPushButton>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QMenuBar>
#include <QTreeWidget>
#include <QSplitter>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("LightCell");
    setWindowIcon(QIcon(":/icons/lightcell.png"));
    resize(1024, 768);
    
    m_model = new SpreadsheetModel(this);
    
    setupUi();
    applyStyles();
}

void MainWindow::setupUi() {
    QMenuBar *menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    QMenu *fileMenu = menuBar->addMenu("File");
    QAction *newAction = fileMenu->addAction("📄 New");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newDocument);

    QAction *openAction = fileMenu->addAction("📂 Open");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFileAction);
    
    QAction *saveAction = fileMenu->addAction("💾 Save");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile);

    QAction *saveAsAction = fileMenu->addAction("📑 Save As...");
    saveAsAction->setShortcuts({QKeySequence::SaveAs, QKeySequence("F12")});
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveFileAs);

    QAction *exportAction = fileMenu->addAction("🚀 Export Excel");
    exportAction->setShortcut(QKeySequence("Ctrl+Shift+E"));
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportToExcel);
    
    QAction *printAction = fileMenu->addAction("🖨️ Print");
    printAction->setShortcut(QKeySequence("Ctrl+P"));
    printAction->setShortcutContext(Qt::WindowShortcut);
    connect(printAction, &QAction::triggered, this, &MainWindow::printSheet);
    addAction(printAction);

    QMenu *editMenu = menuBar->addMenu("Edit");
    QAction *undoAct = editMenu->addAction("↩️ Undo");
    undoAct->setShortcuts({QKeySequence::Undo, QKeySequence("Ctrl+Z")});
    undoAct->setShortcutContext(Qt::WidgetShortcut);
    connect(undoAct, &QAction::triggered, this, [this]() {
        if (m_model) {
            m_model->undo();
            restoreMergedRanges();
            if (m_tableView) {
                QPair<int, int> pos = m_model->getCursorPos();
                if (pos.first >= 0 && pos.second >= 0 && pos.first < m_model->rowCount() && pos.second < m_model->columnCount()) {
                    m_tableView->setCurrentIndex(m_model->index(pos.first, pos.second));
                }
                m_tableView->viewport()->update();
                m_tableView->update();
            }
        }
    });

    QAction *redoAct = editMenu->addAction("↪️ Redo");
    redoAct->setShortcuts({QKeySequence::Redo, QKeySequence("Ctrl+Shift+Z"), QKeySequence("Ctrl+Y")});
    redoAct->setShortcutContext(Qt::WidgetShortcut);
    connect(redoAct, &QAction::triggered, this, [this]() {
        if (m_model) {
            m_model->redo();
            restoreMergedRanges();
            if (m_tableView) {
                QPair<int, int> pos = m_model->getCursorPos();
                if (pos.first >= 0 && pos.second >= 0 && pos.first < m_model->rowCount() && pos.second < m_model->columnCount()) {
                    m_tableView->setCurrentIndex(m_model->index(pos.first, pos.second));
                }
                m_tableView->viewport()->update();
                m_tableView->update();
            }
        }
    });
    
    QAction *findAction = editMenu->addAction("🔍 Find");
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &MainWindow::applyFind);

    QToolBar *toolBar = addToolBar("MainToolBar");
    toolBar->setMovable(false);

    QAction *boldAction = toolBar->addAction("B");
    boldAction->setToolTip("굵게");
    boldAction->setShortcut(QKeySequence::Bold);
    connect(boldAction, &QAction::triggered, this, &MainWindow::applyBold);
    
    QAction *italicAction = toolBar->addAction("I");
    italicAction->setToolTip("기울임");
    italicAction->setShortcut(QKeySequence::Italic);
    connect(italicAction, &QAction::triggered, this, &MainWindow::applyItalic);
    
    m_fontSizeCombo = new QComboBox(this);
    m_fontSizeCombo->setToolTip("Font Size");
    m_fontSizeCombo->setEditable(true);
    m_fontSizeCombo->addItems({"8", "9", "10", "11", "12", "14", "16", "18", "20", "22", "24", "28", "36", "48", "72"});
    
    connect(m_fontSizeCombo->lineEdit(), &QLineEdit::returnPressed, this, [this]() {
        bool ok;
        int size = m_fontSizeCombo->currentText().toInt(&ok);
        if (ok && size > 0) {
            changeFontSize(size);
            m_tableView->setFocus();
        }
    });
    connect(m_fontSizeCombo, &QComboBox::activated, this, [this](int index) {
        bool ok;
        int size = m_fontSizeCombo->itemText(index).toInt(&ok);
        if (ok && size > 0) {
            changeFontSize(size);
            m_tableView->setFocus();
        }
    });
    toolBar->addWidget(m_fontSizeCombo);
    
    toolBar->addSeparator();
    
    QAction *alignLeftAct = toolBar->addAction("⬅️");
    alignLeftAct->setToolTip("좌로 정렬");
    alignLeftAct->setShortcut(QKeySequence("Ctrl+L"));
    connect(alignLeftAct, &QAction::triggered, this, &MainWindow::applyAlignLeft);
    QAction *alignCenterAct = toolBar->addAction("⬆️");
    alignCenterAct->setToolTip("가운데 정렬");
    alignCenterAct->setShortcut(QKeySequence("Ctrl+E"));
    connect(alignCenterAct, &QAction::triggered, this, &MainWindow::applyAlignCenter);
    QAction *alignRightAct = toolBar->addAction("➡️");
    alignRightAct->setToolTip("우로 정렬");
    alignRightAct->setShortcut(QKeySequence("Ctrl+R"));
    connect(alignRightAct, &QAction::triggered, this, &MainWindow::applyAlignRight);

    toolBar->addSeparator();

    QAction *bgAct = toolBar->addAction("🎨");
    bgAct->setToolTip("배경색");
    bgAct->setShortcut(QKeySequence("Alt+B"));
    connect(bgAct, &QAction::triggered, this, &MainWindow::applyBackgroundColor);
    QAction *fgAct = toolBar->addAction("🖍️");
    fgAct->setToolTip("글자색");
    fgAct->setShortcut(QKeySequence("Alt+C"));
    connect(fgAct, &QAction::triggered, this, &MainWindow::applyTextColor);
    QAction *mergeAct = toolBar->addAction("🔗");
    mergeAct->setToolTip("병합/해제");
    mergeAct->setShortcut(QKeySequence("Ctrl+M"));
    connect(mergeAct, &QAction::triggered, this, &MainWindow::applyMerge);
    
    toolBar->addSeparator();
    
    QAction *filterAction = toolBar->addAction("▼");
    filterAction->setToolTip("Filter");
    filterAction->setShortcut(QKeySequence("Ctrl+Shift+L"));
    connect(filterAction, &QAction::triggered, this, &MainWindow::applyFilter);

    toolBar->addSeparator();

    QToolButton *borderBtn = new QToolButton(this);
    borderBtn->setText("⊞");
    borderBtn->setToolTip("테두리");
    borderBtn->setPopupMode(QToolButton::InstantPopup);
    QMenu *borderMenu = new QMenu(this);
    borderMenu->addAction("테두리 없음", this, [this]() { applyBorders("None"); });
    borderMenu->addSeparator();
    borderMenu->addAction("테두리 상", this, [this]() { applyBorders("ThinTop"); });
    borderMenu->addAction("테두리 하", this, [this]() { applyBorders("ThinBottom"); });
    borderMenu->addAction("테두리 좌", this, [this]() { applyBorders("ThinLeft"); });
    borderMenu->addAction("테두리 우", this, [this]() { applyBorders("ThinRight"); });
    borderMenu->addAction("테두리 바깥", this, [this]() { applyBorders("ThinOutside"); });
    borderMenu->addAction("테두리 안쪽", this, [this]() { applyBorders("ThinInside"); });
    borderMenu->addAction("테두리 전체", this, [this]() { applyBorders("ThinAll"); });
    borderMenu->addSeparator();

    // 굵은 선
    borderMenu->addAction("굵은 테두리 상", this, [this]() { applyBorders("ThickTop"); });
    borderMenu->addAction("굵은 테두리 하", this, [this]() { applyBorders("ThickBottom"); });
    borderMenu->addAction("굵은 테두리 좌", this, [this]() { applyBorders("ThickLeft"); });
    borderMenu->addAction("굵은 테두리 우", this, [this]() { applyBorders("ThickRight"); });
    borderMenu->addAction("굵은 테두리 바깥", this, [this]() { applyBorders("ThickOutside"); });
    borderMenu->addAction("굵은 테두리 안쪽", this, [this]() { applyBorders("ThickInside"); });
    borderMenu->addAction("굵은 테두리 전체", this, [this]() { applyBorders("ThickAll"); });
    borderMenu->addSeparator();

    // 이중선
    borderMenu->addAction("이중선 테두리 상", this, [this]() { applyBorders("DoubleTop"); });
    borderMenu->addAction("이중선 테두리 하", this, [this]() { applyBorders("DoubleBottom"); });
    borderMenu->addAction("이중선 테두리 좌", this, [this]() { applyBorders("DoubleLeft"); });
    borderMenu->addAction("이중선 테두리 우", this, [this]() { applyBorders("DoubleRight"); });
    borderMenu->addAction("이중선 테두리 바깥", this, [this]() { applyBorders("DoubleOutside"); });
    borderMenu->addAction("이중선 테두리 안쪽", this, [this]() { applyBorders("DoubleInside"); });
    borderMenu->addAction("이중선 테두리 전체", this, [this]() { applyBorders("DoubleAll"); });

    borderBtn->setMenu(borderMenu);
    toolBar->addWidget(borderBtn);

    QAction *verticalTextAct = toolBar->addAction("↕️");
    verticalTextAct->setToolTip("세로쓰기");
    verticalTextAct->setCheckable(true);
    connect(verticalTextAct, &QAction::triggered, this, [this, verticalTextAct](bool checked) {
        if (!m_tableView || !m_tableView->selectionModel()) return;
        SpreadsheetModel *m = qobject_cast<SpreadsheetModel*>(m_tableView->model());
        if (m) m->applyVerticalText(m_tableView->selectionModel()->selectedIndexes(), checked);
    });

    toolBar->addSeparator();
    QComboBox *formatCombo = new QComboBox(this);
    formatCombo->setToolTip("표시형식");
    formatCombo->addItems({"일반", "회계"});
    connect(formatCombo, &QComboBox::activated, this, [this, formatCombo](int index) {
        if (!m_tableView || !m_tableView->selectionModel()) return;
        SpreadsheetModel *m = qobject_cast<SpreadsheetModel*>(m_tableView->model());
        if (m) m->applyFormat(m_tableView->selectionModel()->selectedIndexes(), index == 1 ? "Accounting" : "General");
        m_tableView->viewport()->update();
    });
    toolBar->addWidget(formatCombo);

    // 글로벌 단축키 추가 (클립보드 및 그리드/시트 조작)
    QAction *copyAct = new QAction(this);
    copyAct->setShortcut(QKeySequence::Copy);
    connect(copyAct, &QAction::triggered, this, &MainWindow::applyCopy);
    addAction(copyAct);
    editMenu->addAction(copyAct);
    copyAct->setText("Copy");

    QAction *cutAct = new QAction(this);
    cutAct->setShortcut(QKeySequence::Cut);
    connect(cutAct, &QAction::triggered, this, &MainWindow::applyCut);
    addAction(cutAct);
    editMenu->addAction(cutAct);
    cutAct->setText("Cut");

    QAction *pasteAct = new QAction(this);
    pasteAct->setShortcut(QKeySequence::Paste);
    connect(pasteAct, &QAction::triggered, this, &MainWindow::applyPaste);
    addAction(pasteAct);
    editMenu->addAction(pasteAct);
    pasteAct->setText("Paste");

    QAction *insAct = new QAction(this);
    insAct->setShortcuts({QKeySequence("Ctrl+Shift++"), QKeySequence("Ctrl++"), QKeySequence("Ctrl+=")});
    connect(insAct, &QAction::triggered, this, [this]() {
        if (!m_tableView) return;
        QModelIndexList selectedCols = m_tableView->selectionModel()->selectedColumns();
        if (!selectedCols.isEmpty()) {
            insertCol();
        } else {
            insertRow();
        }
    });
    addAction(insAct);

    QAction *delAct = new QAction(this);
    delAct->setShortcut(QKeySequence("Ctrl+-"));
    connect(delAct, &QAction::triggered, this, [this]() {
        if (!m_tableView) return;
        QModelIndexList selectedCols = m_tableView->selectionModel()->selectedColumns();
        if (!selectedCols.isEmpty()) {
            deleteCol();
        } else {
            deleteRow();
        }
    });
    addAction(delAct);

    QAction *addSheetAct = new QAction(this);
    addSheetAct->setShortcut(QKeySequence("Ctrl+T"));
    connect(addSheetAct, &QAction::triggered, this, &MainWindow::addNewSheet);
    addAction(addSheetAct);
    
    auto getSheetNamesFromTree = [this]() -> QStringList {
        QStringList names;
        QTreeWidgetItemIterator it(m_sheetTree);
        while (*it) {
            if ((*it)->data(0, Qt::UserRole).toString() == "sheet") {
                names.append((*it)->text(0));
            }
            ++it;
        }
        return names;
    };

    QAction *prevSheetAct = new QAction(this);
    prevSheetAct->setShortcuts({QKeySequence("Ctrl+Shift+Tab"), QKeySequence("Ctrl+PageUp"), QKeySequence("Ctrl+Shift+Backtab")});
    prevSheetAct->setShortcutContext(Qt::ApplicationShortcut);
    connect(prevSheetAct, &QAction::triggered, this, [this, getSheetNamesFromTree]() {
        QString currentName = sheetTabs->tabText(sheetTabs->currentIndex());
        QStringList names = getSheetNamesFromTree();
        int idx = names.indexOf(currentName);
        if (idx > 0) {
            QString targetName = names[idx - 1];
            for (int i = 0; i < sheetTabs->count(); ++i) {
                if (sheetTabs->tabText(i) == targetName) {
                    sheetTabs->setCurrentIndex(i);
                    break;
                }
            }
        }
    });
    addAction(prevSheetAct);

    QAction *nextSheetAct = new QAction(this);
    nextSheetAct->setShortcuts({QKeySequence("Ctrl+Tab"), QKeySequence("Ctrl+PageDown")});
    nextSheetAct->setShortcutContext(Qt::ApplicationShortcut);
    connect(nextSheetAct, &QAction::triggered, this, [this, getSheetNamesFromTree]() {
        QString currentName = sheetTabs->tabText(sheetTabs->currentIndex());
        QStringList names = getSheetNamesFromTree();
        int idx = names.indexOf(currentName);
        if (idx >= 0 && idx < names.size() - 1) {
            QString targetName = names[idx + 1];
            for (int i = 0; i < sheetTabs->count(); ++i) {
                if (sheetTabs->tabText(i) == targetName) {
                    sheetTabs->setCurrentIndex(i);
                    break;
                }
            }
        }
    });
    addAction(nextSheetAct);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_splitter);
    
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);
    
    QWidget *treeToolbar = new QWidget(leftPanel);
    QHBoxLayout *treeToolbarLayout = new QHBoxLayout(treeToolbar);
    treeToolbarLayout->setContentsMargins(2, 2, 2, 2);
    QPushButton *btnNewFolder = new QPushButton("📁", treeToolbar);
    btnNewFolder->setToolTip("새 폴더");
    btnNewFolder->setFlat(true);
    QPushButton *btnNewSheet = new QPushButton("📄", treeToolbar);
    btnNewSheet->setToolTip("새 시트");
    btnNewSheet->setFlat(true);
    treeToolbarLayout->addWidget(btnNewFolder);
    treeToolbarLayout->addWidget(btnNewSheet);
    treeToolbarLayout->addStretch();
    
    leftLayout->addWidget(treeToolbar);
    
    m_sheetTree = new QTreeWidget(leftPanel);
    m_sheetTree->setHeaderHidden(true);
    m_sheetTree->setDragEnabled(true);
    m_sheetTree->setAcceptDrops(true);
    m_sheetTree->setDropIndicatorShown(true);
    m_sheetTree->setDragDropMode(QAbstractItemView::InternalMove);
    m_sheetTree->setContextMenuPolicy(Qt::CustomContextMenu);
    
    leftLayout->addWidget(m_sheetTree);
    m_splitter->addWidget(leftPanel);
    
    sheetTabs = new QTabWidget(this);
    sheetTabs->tabBar()->hide();
    m_splitter->addWidget(sheetTabs);
    m_splitter->setSizes({120, 880});
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    
    connect(btnNewFolder, &QPushButton::clicked, this, &MainWindow::addNewFolder);
    connect(btnNewSheet, &QPushButton::clicked, this, &MainWindow::addNewSheet);
    connect(m_sheetTree, &QTreeWidget::itemClicked, this, &MainWindow::onTreeItemClicked);
    connect(m_sheetTree, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onTreeItemDoubleClicked);
    connect(m_sheetTree, &QWidget::customContextMenuRequested, this, &MainWindow::showTreeContextMenu);
    connect(sheetTabs, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    m_tableView = createSheetTab(m_model, "Sheet1");
    syncTreeToTabs();
}
CustomTableView* MainWindow::currentView() const {
    return qobject_cast<CustomTableView*>(sheetTabs->currentWidget());
}

SpreadsheetModel* MainWindow::currentModel() const {
    CustomTableView *view = currentView();
    if (view) return qobject_cast<SpreadsheetModel*>(view->model());
    return nullptr;
}

CustomTableView* MainWindow::createSheetTab(SpreadsheetModel *model, const QString &title) {
    CustomTableView *view = new CustomTableView(this);
    view->setShowGrid(false); // 커스텀 격자 렌더링을 위해 기본 격자 비활성화
    view->setItemDelegate(new SpreadsheetDelegate(this));
    view->setModel(model);
    view->verticalHeader()->setDefaultSectionSize(25);
    view->verticalHeader()->setMinimumSectionSize(5);
    view->horizontalHeader()->setDefaultSectionSize(100);
    view->horizontalHeader()->setMinimumSectionSize(5);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view, &QTableView::customContextMenuRequested, this, &MainWindow::showContextMenu);

    connect(model, &QAbstractItemModel::modelReset, this, [view, model]() {
        view->clearSpans();
        for (const QRect &r : model->getMergedRanges()) {
            view->setSpan(r.y(), r.x(), r.height(), r.width());
        }
    });

    connect(model, &SpreadsheetModel::sizesRestored, this, [view, model]() {
        QHash<int, int> rows = model->getRowHeights();
        for (auto it = rows.begin(); it != rows.end(); ++it) {
            view->setRowHeight(it.key(), it.value());
        }
        QHash<int, int> cols = model->getColWidths();
        for (auto it = cols.begin(); it != cols.end(); ++it) {
            view->setColumnWidth(it.key(), it.value());
        }
    });

    QTimer *resizeTimer = new QTimer(view);
    resizeTimer->setSingleShot(true);
    resizeTimer->setInterval(300);
    bool *resizeUndoPushed = new bool(false);
    bool *isSyncResizing = new bool(false);

    connect(resizeTimer, &QTimer::timeout, this, [resizeUndoPushed]() {
        *resizeUndoPushed = false;
    });

    connect(view->horizontalHeader(), &QHeaderView::sectionResized, this, [view, model, resizeTimer, resizeUndoPushed, isSyncResizing](int logicalIndex, int oldSize, int newSize) {
        if (*isSyncResizing) return;
        if (!(*resizeUndoPushed)) {
            model->pushUndo();
            *resizeUndoPushed = true;
        }
        
        bool isSelected = false;
        QModelIndexList selectedCols = view->selectionModel()->selectedColumns();
        for (const QModelIndex &idx : selectedCols) {
            if (idx.column() == logicalIndex) { isSelected = true; break; }
        }
        
        *isSyncResizing = true;
        if (isSelected && selectedCols.size() > 1) {
            for (const QModelIndex &idx : selectedCols) {
                int c = idx.column();
                if (c != logicalIndex) {
                    view->horizontalHeader()->resizeSection(c, newSize);
                    model->setColWidth(c, newSize);
                }
            }
        }
        model->setColWidth(logicalIndex, newSize);
        *isSyncResizing = false;
        resizeTimer->start();
    });

    connect(view->verticalHeader(), &QHeaderView::sectionResized, this, [view, model, resizeTimer, resizeUndoPushed, isSyncResizing](int logicalIndex, int oldSize, int newSize) {
        if (*isSyncResizing) return;
        if (!(*resizeUndoPushed)) {
            model->pushUndo();
            *resizeUndoPushed = true;
        }

        bool isSelected = false;
        QModelIndexList selectedRows = view->selectionModel()->selectedRows();
        for (const QModelIndex &idx : selectedRows) {
            if (idx.row() == logicalIndex) { isSelected = true; break; }
        }

        *isSyncResizing = true;
        if (isSelected && selectedRows.size() > 1) {
            for (const QModelIndex &idx : selectedRows) {
                int r = idx.row();
                if (r != logicalIndex) {
                    view->verticalHeader()->resizeSection(r, newSize);
                    model->setRowHeight(r, newSize);
                }
            }
        }
        model->setRowHeight(logicalIndex, newSize);
        *isSyncResizing = false;
        resizeTimer->start();
    });

    connect(view->horizontalHeader(), &QHeaderView::sectionDoubleClicked, this, [view](int logicalIndex) {
        QModelIndexList selected = view->selectionModel()->selectedIndexes();
        QSet<int> cols;
        for (const QModelIndex &idx : selected) cols.insert(idx.column());
        
        if (cols.contains(logicalIndex) && cols.size() > 1) {
            for (int c : cols) view->resizeColumnToContents(c);
        } else {
            view->resizeColumnToContents(logicalIndex);
        }
    });

    connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this, [this, model](const QModelIndex &current, const QModelIndex &) {
        if (!current.isValid()) return;
        QFont f = model->data(current, Qt::FontRole).value<QFont>();
        m_fontSizeCombo->blockSignals(true);
        m_fontSizeCombo->setCurrentText(QString::number(f.pointSize() > 0 ? f.pointSize() : 10));
        m_fontSizeCombo->blockSignals(false);
    });

    int idx = sheetTabs->addTab(view, title);
    sheetTabs->setCurrentIndex(idx);
    return view;
}

void MainWindow::onTabChanged(int index) {
    if (index >= 0) {
        m_tableView = qobject_cast<CustomTableView*>(sheetTabs->widget(index));
        if (m_tableView) {
            m_model = qobject_cast<SpreadsheetModel*>(m_tableView->model());
            restoreMergedRanges();
        }
        syncTreeToTabs();
    }
}

void MainWindow::newDocument() {
    while (sheetTabs->count() > 0) {
        QWidget *w = sheetTabs->widget(0);
        sheetTabs->removeTab(0);
        if (w) w->deleteLater();
    }
    m_model = new SpreadsheetModel(this);
    m_tableView = createSheetTab(m_model, "Sheet1");
    setWindowTitle("LightCell - 새 문서");
    populateTreeFromTabs();
}

void MainWindow::addNewSheet() {
    int num = 1;
    QSet<QString> names;
    for (int i = 0; i < sheetTabs->count(); ++i) {
        names.insert(sheetTabs->tabText(i));
    }
    while (names.contains(QString("Sheet%1").arg(num))) {
        num++;
    }
    SpreadsheetModel *newModel = new SpreadsheetModel(this);
    createSheetTab(newModel, QString("Sheet%1").arg(num));
    syncTreeToTabs();
}

SpreadsheetModel* MainWindow::getSheetModel(const QString &sheetName) {
    for (int i = 0; i < sheetTabs->count(); ++i) {
        if (sheetTabs->tabText(i) == sheetName) {
            CustomTableView *view = qobject_cast<CustomTableView*>(sheetTabs->widget(i));
            if (view) {
                return qobject_cast<SpreadsheetModel*>(view->model());
            }
        }
    }
    return nullptr;
}

void MainWindow::duplicateSheet(int index) {
    if (index < 0) index = sheetTabs->currentIndex();
    if (index < 0 || index >= sheetTabs->count()) return;

    CustomTableView *srcView = qobject_cast<CustomTableView*>(sheetTabs->widget(index));
    SpreadsheetModel *srcModel = srcView ? qobject_cast<SpreadsheetModel*>(srcView->model()) : nullptr;
    if (!srcView || !srcModel) return;

    SpreadsheetModel *newModel = new SpreadsheetModel(this);
    newModel->copyFrom(srcModel);

    QString origName = sheetTabs->tabText(index);
    QString newName = origName + " (복사본)";
    int counter = 2;
    QSet<QString> names;
    for (int i = 0; i < sheetTabs->count(); ++i) names.insert(sheetTabs->tabText(i));
    while (names.contains(newName)) {
        newName = origName + QString(" (복사본%1)").arg(counter++);
    }

    CustomTableView *newView = createSheetTab(newModel, newName);
    for (int c = 0; c < qMin(srcModel->columnCount(), 1000); ++c) {
        if (srcView->columnWidth(c) != 100) {
            newView->setColumnWidth(c, srcView->columnWidth(c));
        }
    }
    for (int r = 0; r < qMin(srcModel->rowCount(), 1000); ++r) {
        if (srcView->rowHeight(r) != 25) {
            newView->setRowHeight(r, srcView->rowHeight(r));
        }
    }
    int newIdx = sheetTabs->indexOf(newView);
    if (newIdx != index + 1) {
        sheetTabs->tabBar()->moveTab(newIdx, index + 1);
    }
    sheetTabs->setCurrentWidget(newView);
    syncTreeToTabs();
}

void MainWindow::deleteSheet(int index) {
    if (index < 0) index = sheetTabs->currentIndex();
    if (index < 0 || index >= sheetTabs->count()) return;

    if (sheetTabs->count() <= 1) {
        QMessageBox::warning(this, "시트 삭제 불가", "최소 1개의 시트는 유지해야 합니다.");
        return;
    }

    auto ans = QMessageBox::question(this, "시트 삭제", 
        QString("'%1' 시트를 정말 삭제하시겠습니까?").arg(sheetTabs->tabText(index)),
        QMessageBox::Yes | QMessageBox::No);
    if (ans != QMessageBox::Yes) return;

    QWidget *w = sheetTabs->widget(index);
    sheetTabs->removeTab(index);
    if (w) w->deleteLater();
    syncTreeToTabs();
}

void MainWindow::renameSheet(int index) {
    if (index < 0) index = sheetTabs->currentIndex();
    if (index < 0 || index >= sheetTabs->count()) return;

    bool ok = false;
    QString oldName = sheetTabs->tabText(index);
    QString newName = QInputDialog::getText(this, "시트 이름 바꾸기", "새 시트 이름:",
                                          QLineEdit::Normal, oldName, &ok);
    if (ok && !newName.trimmed().isEmpty()) {
        newName = newName.trimmed();
        sheetTabs->setTabText(index, newName);
        QTreeWidgetItemIterator it(m_sheetTree);
        while (*it) {
            if ((*it)->data(0, Qt::UserRole).toString() == "sheet" && (*it)->text(0) == oldName) {
                (*it)->setText(0, newName);
                break;
            }
            ++it;
        }
    }
}

void MainWindow::showTabContextMenu(const QPoint &pos) {
    int tabIndex = sheetTabs->tabBar()->tabAt(sheetTabs->tabBar()->mapFrom(sheetTabs, pos));
    QMenu menu(this);
    
    QAction *addAction = menu.addAction("➕ 새 시트 추가");
    connect(addAction, &QAction::triggered, this, &MainWindow::addNewSheet);

    if (tabIndex >= 0) {
        QAction *dupAction = menu.addAction("📑 시트 복제");
        connect(dupAction, &QAction::triggered, this, [this, tabIndex]() { duplicateSheet(tabIndex); });

        QAction *renameAction = menu.addAction("✏️ 이름 바꾸기");
        connect(renameAction, &QAction::triggered, this, [this, tabIndex]() { renameSheet(tabIndex); });

        QAction *delAction = menu.addAction("🗑️ 시트 삭제");
        connect(delAction, &QAction::triggered, this, [this, tabIndex]() { deleteSheet(tabIndex); });
    }

    menu.exec(sheetTabs->mapToGlobal(pos));
}

void MainWindow::onOpenFileAction() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open File", "", "LightCell & CSV Files (*.cx *.csv);;LightCell Files (*.cx);;CSV Files (*.csv);;All Files (*.*)");
    if (!fileName.isEmpty()) {
        openFile(fileName);
    }
}

void MainWindow::openFile(const QString &fileName) {
    if (!fileName.isEmpty()) {
        bool success = false;
        if (fileName.endsWith(".cx", Qt::CaseInsensitive)) {
            success = loadProjectCx(fileName);
        } else {
            while (sheetTabs->count() > 0) {
                QWidget *w = sheetTabs->widget(0);
                sheetTabs->removeTab(0);
                if (w) w->deleteLater();
            }
            SpreadsheetModel *newModel = new SpreadsheetModel(this);
            success = newModel->loadCsv(fileName);
            createSheetTab(newModel, QFileInfo(fileName).fileName());
            populateTreeFromTabs();
        }
        if (success) {
            m_currentFilePath = fileName;
            setWindowTitle("LightCell - " + fileName);
            restoreMergedRanges();
            syncTreeToTabs();
        } else {
            QMessageBox::warning(this, "Error", "파일을 여는 데 실패했습니다.");
        }
    }
}

void MainWindow::saveFile() {
    if (m_tableView) m_tableView->commitCurrentEditor();
    if (m_currentFilePath.isEmpty()) {
        saveFileAs();
        return;
    }

    bool success = false;
    if (m_currentFilePath.endsWith(".cx", Qt::CaseInsensitive)) {
        success = saveProjectCx(m_currentFilePath);
    } else if (m_currentFilePath.endsWith(".csv", Qt::CaseInsensitive)) {
        SpreadsheetModel *model = currentModel();
        if (model) success = model->saveCsv(m_currentFilePath);
    } else {
        saveFileAs();
        return;
    }

    if (success) {
        setWindowTitle("LightCell - " + m_currentFilePath);
        QMessageBox::information(this, "Save", "파일이 성공적으로 저장되었습니다.");
    } else {
        QMessageBox::warning(this, "Error", "파일 저장에 실패했습니다.");
    }
}

void MainWindow::saveFileAs() {
    if (m_tableView) m_tableView->commitCurrentEditor();
    QString defaultFilter = "LightCell Project (*.cx)";
    QString fileName = QFileDialog::getSaveFileName(this, "Save File As", m_currentFilePath, "LightCell Project (*.cx);;CSV File (*.csv)", &defaultFilter);
    if (!fileName.isEmpty()) {
        bool success = false;
        if (fileName.endsWith(".cx", Qt::CaseInsensitive)) {
            success = saveProjectCx(fileName);
        } else if (fileName.endsWith(".csv", Qt::CaseInsensitive)) {
            SpreadsheetModel *model = currentModel();
            if (model) success = model->saveCsv(fileName);
        } else {
            if (defaultFilter.contains(".cx")) {
                fileName += ".cx";
                success = saveProjectCx(fileName);
            } else {
                fileName += ".csv";
                SpreadsheetModel *model = currentModel();
                if (model) success = model->saveCsv(fileName);
            }
        }

        if (success) {
            m_currentFilePath = fileName;
            if (fileName.endsWith(".csv", Qt::CaseInsensitive) && sheetTabs->count() > 0) {
                sheetTabs->setTabText(sheetTabs->currentIndex(), QFileInfo(fileName).fileName());
            }
            setWindowTitle("LightCell - " + fileName);
            QMessageBox::information(this, "Save As", "파일이 성공적으로 저장되었습니다.");
        } else {
            QMessageBox::warning(this, "Error", "파일 저장에 실패했습니다.");
        }
    }
}

void MainWindow::exportToExcel() {
    if (m_tableView) m_tableView->commitCurrentEditor();
    QString fileName = QFileDialog::getSaveFileName(this, "Export to Excel", "", "Excel XML Worksheet (*.xls *.xml)");
    if (!fileName.isEmpty()) {
        if (!fileName.endsWith(".xls", Qt::CaseInsensitive) && !fileName.endsWith(".xml", Qt::CaseInsensitive)) {
            fileName += ".xls";
        }
        if (exportProjectToExcel(fileName)) {
            QMessageBox::information(this, "Export", "엑셀 파일로 성공적으로 내보냈습니다!\n" + fileName);
        } else {
            QMessageBox::warning(this, "Error", "엑셀 파일 내보내기에 실패했습니다.");
        }
    }
}

bool MainWindow::saveProjectCx(const QString &filePath) {
    if (m_tableView) m_tableView->commitCurrentEditor();
    QString tmpPath = filePath + ".tmp";
    QFile outFile(tmpPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QJsonObject root;
    root["version"] = "2.0";
    root["activeSheet"] = sheetTabs->currentIndex();
    root["treeStructure"] = serializeTree();

    QJsonArray sheetsArray;
    for (int i = 0; i < sheetTabs->count(); ++i) {
        CustomTableView *view = qobject_cast<CustomTableView*>(sheetTabs->widget(i));
        SpreadsheetModel *model = view ? qobject_cast<SpreadsheetModel*>(view->model()) : nullptr;
        if (!view || !model) continue;

        QJsonObject sheetObj;
        sheetObj["name"] = sheetTabs->tabText(i);
        sheetObj["model"] = model->toJsonObject();

        QJsonArray colWidths;
        for (int c = 0; c < qMin(model->columnCount(), 1000); ++c) {
            if (view->columnWidth(c) != 100) {
                QJsonObject wObj;
                wObj["c"] = c; wObj["w"] = view->columnWidth(c);
                colWidths.append(wObj);
            }
        }
        sheetObj["colWidths"] = colWidths;

        QJsonArray rowHeights;
        for (int r = 0; r < qMin(model->rowCount(), 1000); ++r) {
            if (view->rowHeight(r) != 25) {
                QJsonObject hObj;
                hObj["r"] = r; hObj["h"] = view->rowHeight(r);
                rowHeights.append(hObj);
            }
        }
        sheetObj["rowHeights"] = rowHeights;

        sheetsArray.append(sheetObj);
    }
    root["sheets"] = sheetsArray;

    QJsonDocument doc(root);
    outFile.write(doc.toJson(QJsonDocument::Compact));
    outFile.close();

    if (QFile::exists(filePath)) {
        QFile::remove(filePath);
    }
    bool res = QFile::rename(tmpPath, filePath);
    if (res) {
        loadProjectCx(filePath);
    }
    return res;
}

bool MainWindow::loadProjectCx(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    
    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;

    QJsonObject root = doc.object();
    if (root.contains("sheets")) {
        while (sheetTabs->count() > 0) {
            QWidget *w = sheetTabs->widget(0);
            sheetTabs->removeTab(0);
            if (w) w->deleteLater();
        }

        QJsonArray sheetsArray = root["sheets"].toArray();
        for (const QJsonValue &val : sheetsArray) {
            QJsonObject sheetObj = val.toObject();
            QString name = sheetObj["name"].toString("Sheet1");
            SpreadsheetModel *newModel = new SpreadsheetModel(this);
            newModel->fromJsonObject(sheetObj["model"].toObject());
            CustomTableView *newView = createSheetTab(newModel, name);

            QJsonArray colWidths = sheetObj["colWidths"].toArray();
            for (const QJsonValue &cwVal : colWidths) {
                QJsonObject cw = cwVal.toObject();
                newView->setColumnWidth(cw["c"].toInt(), cw["w"].toInt());
            }
            QJsonArray rowHeights = sheetObj["rowHeights"].toArray();
            for (const QJsonValue &rhVal : rowHeights) {
                QJsonObject rh = rhVal.toObject();
                newView->setRowHeight(rh["r"].toInt(), rh["h"].toInt());
            }
        }
        int active = root["activeSheet"].toInt(0);
        if (active >= 0 && active < sheetTabs->count()) {
            sheetTabs->setCurrentIndex(active);
        }
    } else {
        while (sheetTabs->count() > 0) {
            QWidget *w = sheetTabs->widget(0);
            sheetTabs->removeTab(0);
            if (w) w->deleteLater();
        }
        SpreadsheetModel *newModel = new SpreadsheetModel(this);
        newModel->fromJsonObject(root);
        createSheetTab(newModel, QFileInfo(filePath).fileName());
    }

    if (root.contains("treeStructure")) {
        buildTreeFromJson(root["treeStructure"].toArray());
    } else {
        populateTreeFromTabs();
    }
    
    return true;
}

bool MainWindow::exportProjectToExcel(const QString &filePath) {
    if (m_tableView) m_tableView->commitCurrentEditor();
    QString tmpPath = filePath + ".tmp";
    QFile outFile(tmpPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QXmlStreamWriter xml(&outFile);
    xml.setAutoFormatting(true);
    xml.writeStartDocument("1.0");
    xml.writeProcessingInstruction("mso-application", "progid=\"Excel.Sheet\"");
    
    xml.writeStartElement("Workbook");
    xml.writeAttribute("xmlns", "urn:schemas-microsoft-com:office:spreadsheet");
    xml.writeAttribute("xmlns:o", "urn:schemas-microsoft-com:office:office");
    xml.writeAttribute("xmlns:x", "urn:schemas-microsoft-com:office:excel");
    xml.writeAttribute("xmlns:ss", "urn:schemas-microsoft-com:office:spreadsheet");
    xml.writeAttribute("xmlns:html", "http://www.w3.org/TR/REC-html40");

    xml.writeStartElement("Styles");
    xml.writeStartElement("Style");
    xml.writeAttribute("ss:ID", "Default");
    xml.writeAttribute("ss:Name", "Normal");
    xml.writeEmptyElement("Alignment");
    xml.writeAttribute("ss:Vertical", "Bottom");
    xml.writeEmptyElement("Font");
    xml.writeAttribute("ss:FontName", "Malgun Gothic");
    xml.writeAttribute("ss:Size", "10");
    xml.writeEndElement(); // Style

    QHash<QString, QString> styleToId;
    int styleIdx = 1;
    for (int i = 0; i < sheetTabs->count(); ++i) {
        CustomTableView *view = qobject_cast<CustomTableView*>(sheetTabs->widget(i));
        SpreadsheetModel *model = view ? qobject_cast<SpreadsheetModel*>(view->model()) : nullptr;
        if (model) {
            model->collectStyles(xml, styleToId, styleIdx);
        }
    }
    xml.writeEndElement(); // Styles

    for (int i = 0; i < sheetTabs->count(); ++i) {
        CustomTableView *view = qobject_cast<CustomTableView*>(sheetTabs->widget(i));
        SpreadsheetModel *model = view ? qobject_cast<SpreadsheetModel*>(view->model()) : nullptr;
        if (model) {
            model->exportWorksheetToXml(xml, sheetTabs->tabText(i), styleToId, styleIdx);
        }
    }

    xml.writeEndElement(); // Workbook
    xml.writeEndDocument();
    outFile.close();

    if (QFile::exists(filePath)) {
        QFile::remove(filePath);
    }
    return QFile::rename(tmpPath, filePath);
}

void MainWindow::applyBold() {
    QItemSelectionModel *selection = m_tableView->selectionModel();
    if (selection) {
        m_model->setCellBold(selection->selectedIndexes());
    }
}

void MainWindow::applyItalic() {
    QItemSelectionModel *selection = m_tableView->selectionModel();
    if (selection) {
        m_model->setCellItalic(selection->selectedIndexes());
    }
}

void MainWindow::changeFontSize(int size) {
    QItemSelectionModel *selection = m_tableView->selectionModel();
    if (selection) {
        m_model->setFontSize(selection->selectedIndexes(), size);
    }
}

void MainWindow::applyFind() {
    bool ok;
    QString text = QInputDialog::getText(this, "찾기", "검색할 내용을 입력하세요:", QLineEdit::Normal, "", &ok);
    if (!ok || text.isEmpty()) return;

    QItemSelectionModel *selection = m_tableView->selectionModel();
    QModelIndexList selected = selection->selectedIndexes();

    int startRow = 0, endRow = m_model->rowCount() - 1;
    int startCol = 0, endCol = m_model->columnCount() - 1;
    bool hasRange = selected.size() > 1;

    if (hasRange) {
        startRow = m_model->rowCount(); endRow = 0;
        startCol = m_model->columnCount(); endCol = 0;
        for (const QModelIndex &idx : selected) {
            if (idx.row() < startRow) startRow = idx.row();
            if (idx.row() > endRow) endRow = idx.row();
            if (idx.column() < startCol) startCol = idx.column();
            if (idx.column() > endCol) endCol = idx.column();
        }
    } else {
        if (!selected.isEmpty()) {
            startRow = selected.first().row();
            startCol = selected.first().column() + 1;
        }
    }

    bool found = false;
    for (int r = startRow; r <= endRow; ++r) {
        int cBegin = (r == startRow && !hasRange) ? startCol : (hasRange ? startCol : 0);
        int cEnd = hasRange ? endCol : m_model->columnCount() - 1;
        for (int c = cBegin; c <= cEnd; ++c) {
            QModelIndex idx = m_model->index(r, c);
            QString val = m_model->data(idx, Qt::DisplayRole).toString();
            if (val.contains(text, Qt::CaseInsensitive)) {
                m_tableView->setCurrentIndex(idx);
                m_tableView->scrollTo(idx);
                found = true;
                break;
            }
        }
        if (found) break;
    }
    if (!found) {
        QMessageBox::information(this, "찾기 결과", "검색된 결과가 더 이상 없습니다.");
    }
}

void MainWindow::applyFilter() {
    // 이미 필터가 켜져 있으면 필터를 끕니다 (토글 기능)
    if (m_model->hasFilter()) {
        m_model->clearFilter();
        m_tableView->viewport()->update();
        return;
    }

    QItemSelectionModel *selection = m_tableView->selectionModel();
    if (!selection || selection->selectedIndexes().isEmpty()) {
        QMessageBox::information(this, "필터 지정", "필터를 적용할 헤더 범위를 가로로 선택해주세요. (예: A3~D3)");
        return;
    }

    QModelIndexList selected = selection->selectedIndexes();
    int row = selected.first().row();
    int minCol = selected.first().column();
    int maxCol = selected.first().column();

    for (const QModelIndex &idx : selected) {
        if (idx.row() != row) {
            QMessageBox::warning(this, "필터 지정 에러", "단일 행에서 열 범위를 선택해야 합니다.");
            return;
        }
        if (idx.column() < minCol) minCol = idx.column();
        if (idx.column() > maxCol) maxCol = idx.column();
    }

    m_model->setFilterRange(row, minCol, maxCol);
    m_tableView->viewport()->update();
}

void MainWindow::restoreMergedRanges() {
    m_tableView->clearSpans();
    for (const QRect &r : m_model->getMergedRanges()) {
        m_tableView->setSpan(r.y(), r.x(), r.height(), r.width());
    }
}

void MainWindow::applyAlignLeft() {
    QItemSelectionModel *selection = m_tableView->selectionModel();
    if (selection) m_model->setCellAlignment(selection->selectedIndexes(), Qt::AlignLeft | Qt::AlignVCenter);
}
void MainWindow::applyAlignCenter() {
    QItemSelectionModel *selection = m_tableView->selectionModel();
    if (selection) m_model->setCellAlignment(selection->selectedIndexes(), Qt::AlignHCenter | Qt::AlignVCenter);
}
void MainWindow::applyAlignRight() {
    QItemSelectionModel *selection = m_tableView->selectionModel();
    if (selection) m_model->setCellAlignment(selection->selectedIndexes(), Qt::AlignRight | Qt::AlignVCenter);
}
void MainWindow::applyBackgroundColor() {
    QItemSelectionModel *selection = m_tableView->selectionModel();
    if (!selection || selection->selectedIndexes().isEmpty()) return;
    QColor c = QColorDialog::getColor(Qt::white, this, "셀 배경색 선택");
    if (c.isValid()) m_model->setCellBackgroundColor(selection->selectedIndexes(), c);
}
void MainWindow::applyTextColor() {
    QItemSelectionModel *selection = m_tableView->selectionModel();
    if (!selection || selection->selectedIndexes().isEmpty()) return;
    QColor c = QColorDialog::getColor(Qt::black, this, "글자 색상 선택");
    if (c.isValid()) m_model->setCellTextColor(selection->selectedIndexes(), c);
}
void MainWindow::applyMerge() {
    QModelIndexList selected = m_tableView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    
    int minR = INT_MAX, maxR = -1, minC = INT_MAX, maxC = -1;
    for (const QModelIndex &idx : selected) {
        if (idx.row() < minR) minR = idx.row();
        if (idx.row() > maxR) maxR = idx.row();
        if (idx.column() < minC) minC = idx.column();
        if (idx.column() > maxC) maxC = idx.column();
    }
    
    int rowSpan = maxR - minR + 1;
    int colSpan = maxC - minC + 1;
    if (rowSpan == 1 && colSpan == 1) return;
    
    QRect targetRange(minC, minR, colSpan, rowSpan);
    
    if (m_model) m_model->beginMacro();
    QList<QRect> toRemove;
    bool exactMatch = false;
    for (const QRect &r : m_model->getMergedRanges()) {
        if (r == targetRange) {
            exactMatch = true;
            toRemove.append(r);
            break;
        } else if (r.intersects(targetRange)) {
            toRemove.append(r);
        }
    }
    for (const QRect &r : toRemove) {
        m_model->removeMergedRange(r);
    }
    if (!exactMatch) {
        m_model->addMergedRange(targetRange);
        QModelIndexList rangeIndexes;
        for (int r = minR; r <= maxR; ++r) {
            for (int c = minC; c <= maxC; ++c) {
                rangeIndexes.append(m_model->index(r, c));
            }
        }
        m_model->setCellAlignment(rangeIndexes, Qt::AlignCenter | Qt::AlignVCenter);
    }
    if (m_model) m_model->endMacro();
    restoreMergedRanges();
}

void MainWindow::showContextMenu(const QPoint &pos) {
    QModelIndex index = m_tableView->indexAt(pos);
    QModelIndexList selected = m_tableView->selectionModel()->selectedIndexes();
    if (selected.isEmpty() && index.isValid()) {
        m_tableView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
    }

    QMenu menu(this);
    
    QAction *cutAct = menu.addAction("✂️ 잘라내기 (Cut)");
    QAction *copyAct = menu.addAction("📋 복사 (Copy)");
    QAction *pasteAct = menu.addAction("📥 붙여넣기 (Paste)");
    menu.addSeparator();

    QAction *insRowAct = menu.addAction("➕ 행 삽입 (Insert Rows Above)");
    QAction *delRowAct = menu.addAction("➖ 행 삭제 (Delete Selected Rows)");
    menu.addSeparator();

    QAction *insColAct = menu.addAction("➕ 열 삽입 (Insert Columns Left)");
    QAction *delColAct = menu.addAction("➖ 열 삭제 (Delete Selected Columns)");
    menu.addSeparator();

    QAction *mergeAct = menu.addAction("🔗 셀 병합 및 가운데 맞춤 (Toggle Merge)");
    QAction *bgAct = menu.addAction("🎨 배경색 지정 (Fill Color)...");
    QAction *fgAct = menu.addAction("🖍️ 글자색 지정 (Text Color)...");

    connect(cutAct, &QAction::triggered, this, &MainWindow::applyCut);
    connect(copyAct, &QAction::triggered, this, &MainWindow::applyCopy);
    connect(pasteAct, &QAction::triggered, this, &MainWindow::applyPaste);
    
    connect(insRowAct, &QAction::triggered, this, &MainWindow::insertRow);
    connect(delRowAct, &QAction::triggered, this, &MainWindow::deleteRow);
    connect(insColAct, &QAction::triggered, this, &MainWindow::insertCol);
    connect(delColAct, &QAction::triggered, this, &MainWindow::deleteCol);
    
    connect(mergeAct, &QAction::triggered, this, &MainWindow::applyMerge);
    connect(bgAct, &QAction::triggered, this, &MainWindow::applyBackgroundColor);
    connect(fgAct, &QAction::triggered, this, &MainWindow::applyTextColor);

    menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}

void MainWindow::insertRow() {
    QModelIndexList selected = m_tableView->selectionModel()->selectedIndexes();
    int row = 0;
    int count = 1;
    if (!selected.isEmpty()) {
        QSet<int> rows;
        for (const QModelIndex &idx : selected) rows.insert(idx.row());
        std::vector<int> sortedRows(rows.begin(), rows.end());
        std::sort(sortedRows.begin(), sortedRows.end());
        row = sortedRows.front();
        count = sortedRows.size();
    } else {
        QModelIndex curr = m_tableView->currentIndex();
        row = curr.isValid() ? curr.row() : 0;
    }
    m_model->insertRows(row, count);
}

void MainWindow::deleteRow() {
    QModelIndexList selected = m_tableView->selectionModel()->selectedIndexes();
    if (m_model) m_model->beginMacro();
    if (!selected.isEmpty()) {
        QSet<int> rowsSet;
        for (const QModelIndex &idx : selected) rowsSet.insert(idx.row());
        std::vector<int> rows(rowsSet.begin(), rowsSet.end());
        std::sort(rows.rbegin(), rows.rend());
        for (int r : rows) m_model->removeRows(r, 1);
    } else {
        QModelIndex curr = m_tableView->currentIndex();
        if (curr.isValid()) m_model->removeRows(curr.row(), 1);
    }
    if (m_model) m_model->endMacro();
    syncTreeToTabs();
}

void MainWindow::insertCol() {
    QModelIndexList selected = m_tableView->selectionModel()->selectedIndexes();
    int col = 0;
    int count = 1;
    if (!selected.isEmpty()) {
        QSet<int> cols;
        for (const QModelIndex &idx : selected) cols.insert(idx.column());
        std::vector<int> sortedCols(cols.begin(), cols.end());
        std::sort(sortedCols.begin(), sortedCols.end());
        col = sortedCols.front();
        count = sortedCols.size();
    } else {
        QModelIndex curr = m_tableView->currentIndex();
        col = curr.isValid() ? curr.column() : 0;
    }
    m_model->insertColumns(col, count);
}

void MainWindow::deleteCol() {
    QModelIndexList selected = m_tableView->selectionModel()->selectedIndexes();
    if (m_model) m_model->beginMacro();
    if (!selected.isEmpty()) {
        QSet<int> colsSet;
        for (const QModelIndex &idx : selected) colsSet.insert(idx.column());
        std::vector<int> cols(colsSet.begin(), colsSet.end());
        std::sort(cols.rbegin(), cols.rend());
        for (int c : cols) m_model->removeColumns(c, 1);
    } else {
        QModelIndex curr = m_tableView->currentIndex();
        if (curr.isValid()) m_model->removeColumns(curr.column(), 1);
    }
    if (m_model) m_model->endMacro();
    syncTreeToTabs();
}

void MainWindow::applyCopy() {
    QModelIndexList selected = m_tableView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    
    int minR = INT_MAX, maxR = -1, minC = INT_MAX, maxC = -1;
    for (const QModelIndex &idx : selected) {
        if (idx.row() < minR) minR = idx.row();
        if (idx.row() > maxR) maxR = idx.row();
        if (idx.column() < minC) minC = idx.column();
        if (idx.column() > maxC) maxC = idx.column();
    }
    
    QString clipText;
    for (int r = minR; r <= maxR; ++r) {
        QStringList rowVals;
        for (int c = minC; c <= maxC; ++c) {
            rowVals.append(m_model->data(m_model->index(r, c), Qt::EditRole).toString());
        }
        clipText += rowVals.join('\t') + (r == maxR ? "" : "\n");
    }
    QApplication::clipboard()->setText(clipText);
}

void MainWindow::applyCut() {
    applyCopy();
    QModelIndexList selected = m_tableView->selectionModel()->selectedIndexes();
    if (m_model) m_model->beginMacro();
    for (const QModelIndex &idx : selected) {
        m_model->setData(idx, "", Qt::EditRole);
    }
    if (m_model) m_model->endMacro();
}

void MainWindow::applyPaste() {
    QString clipText = QApplication::clipboard()->text();
    clipText.remove('\r');
    if (clipText.isEmpty()) return;
    
    QModelIndex curr = m_tableView->currentIndex();
    if (!curr.isValid()) return;
    
    int startRow = curr.row();
    int startCol = curr.column();
    
    if (m_model) m_model->beginMacro();
    
    int r = 0;
    QStringList currentRow;
    QString currentCell;
    bool inQuotes = false;
    
    auto processRow = [&]() {
        if (currentRow.isEmpty() && currentCell.isEmpty()) return;
        currentRow.append(currentCell);
        
        for (int c = 0; c < currentRow.size(); ++c) {
            QString cellData = currentRow[c].trimmed();
            
            QString stripped = cellData;
            stripped.remove(',');
            stripped.remove(QChar(0x20A9)); // '₩'
            stripped.remove('$');
            stripped.remove(QChar(0x20AC)); // '€'
            stripped.remove(' ');
            
            bool ok;
            stripped.toDouble(&ok);
            if (ok && !stripped.isEmpty()) {
                cellData = stripped;
            }
            
            QModelIndex targetIdx = m_model->index(startRow + r, startCol + c);
            if (targetIdx.isValid()) {
                m_model->setData(targetIdx, cellData, Qt::EditRole);
            }
        }
        r++;
        currentRow.clear();
        currentCell.clear();
    };

    for (int i = 0; i < clipText.length(); ++i) {
        QChar c = clipText[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < clipText.length() && clipText[i + 1] == '"') {
                    currentCell += '"';
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                currentCell += c;
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == '\t') {
                currentRow.append(currentCell);
                currentCell.clear();
            } else if (c == '\n') {
                processRow();
            } else if (c == '\r') {
                if (i + 1 < clipText.length() && clipText[i + 1] == '\n') {
                    processRow();
                    ++i;
                } else {
                    processRow();
                }
            } else {
                currentCell += c;
            }
        }
    }
    
    if (!currentCell.isEmpty() || !currentRow.isEmpty()) {
        processRow();
    }
    
    if (m_model) m_model->endMacro();
}

void MainWindow::applyStyles() {
    setStyleSheet(R"(
        QMainWindow { background-color: #F8F9FA; }
        QToolBar {
            background-color: #FFFFFF;
            border-bottom: 1px solid #E0E0E0;
            padding: 5px;
        }
        QTableView {
            background-color: #FFFFFF;
            gridline-color: #E0E0E0;
            selection-background-color: #E8F0FE;
            selection-color: #1A73E8;
            border: none;
        }
        QHeaderView::section {
            background-color: #F8F9FA;
            border: 1px solid #E0E0E0;
            border-top: none;
            border-left: none;
            padding: 4px;
            font-weight: bold;
            color: #5F6368;
        }
        QTabWidget::pane {
            border: 1px solid #E0E0E0;
            border-top: none;
        }
        QTabBar::tab {
            background-color: #F1F3F4;
            border: 1px solid #E0E0E0;
            padding: 6px 15px;
            margin-right: -1px;
        }
        QTabBar::tab:selected {
            background-color: #FFFFFF;
            border-bottom-color: #FFFFFF;
            font-weight: bold;
            color: #1A73E8;
        }
    )");
}

void MainWindow::printSheet() {
    SpreadsheetModel *model = currentModel();
    CustomTableView *view = currentView();
    if (!model) return;

    QDialog previewDlg(this);
    previewDlg.setWindowTitle("LightCell - 인쇄 미리보기 및 설정 (Print Preview & Setup)");
    previewDlg.resize(1150, 800);

    QVBoxLayout *mainLayout = new QVBoxLayout(&previewDlg);

    // Top setup bar
    QHBoxLayout *setupLayout = new QHBoxLayout();
    setupLayout->addWidget(new QLabel("<b>[인쇄 범위]</b>"));
    
    QRadioButton *radioUsed = new QRadioButton("전체 사용 영역");
    QRadioButton *radioSel = new QRadioButton("선택 영역");
    QRadioButton *radioCustom = new QRadioButton("사용자 지정:");
    
    QButtonGroup *rangeGroup = new QButtonGroup(&previewDlg);
    rangeGroup->addButton(radioUsed);
    rangeGroup->addButton(radioSel);
    rangeGroup->addButton(radioCustom);

    setupLayout->addWidget(radioUsed);
    setupLayout->addWidget(radioSel);
    setupLayout->addWidget(radioCustom);

    setupLayout->addWidget(new QLabel("좌상단(시작):"));
    QLineEdit *startEdit = new QLineEdit();
    startEdit->setFixedWidth(55);
    setupLayout->addWidget(startEdit);

    setupLayout->addWidget(new QLabel("우하단(끝):"));
    QLineEdit *endEdit = new QLineEdit();
    endEdit->setFixedWidth(55);
    setupLayout->addWidget(endEdit);

    setupLayout->addSpacing(20);
    setupLayout->addWidget(new QLabel("<b>[옵션]</b>"));
    QCheckBox *chkGrid = new QCheckBox("눈금선");
    chkGrid->setChecked(true);
    setupLayout->addWidget(chkGrid);

    QCheckBox *chkHead = new QCheckBox("행/열 헤더");
    chkHead->setChecked(false);
    setupLayout->addWidget(chkHead);

    QCheckBox *chkFit = new QCheckBox("페이지 너비 맞춤");
    chkFit->setChecked(true);
    setupLayout->addWidget(chkFit);

    setupLayout->addSpacing(20);
    setupLayout->addWidget(new QLabel("<b>[정렬 및 위치]</b>"));
    QCheckBox *chkCenterH = new QCheckBox("가로 가운데 정렬");
    chkCenterH->setChecked(true);
    setupLayout->addWidget(chkCenterH);
    QCheckBox *chkCenterV = new QCheckBox("세로 가운데 정렬");
    chkCenterV->setChecked(false);
    setupLayout->addWidget(chkCenterV);
    
    QSpinBox *spinOffsetX = new QSpinBox();
    spinOffsetX->setRange(-2000, 2000);
    spinOffsetX->setValue(0);
    spinOffsetX->setSuffix(" px");
    QHBoxLayout *hbX = new QHBoxLayout();
    hbX->addWidget(new QLabel("X 오프셋:"));
    hbX->addWidget(spinOffsetX);
    setupLayout->addLayout(hbX);
    
    QSpinBox *spinOffsetY = new QSpinBox();
    spinOffsetY->setRange(-2000, 2000);
    spinOffsetY->setValue(0);
    spinOffsetY->setSuffix(" px");
    QHBoxLayout *hbY = new QHBoxLayout();
    hbY->addWidget(new QLabel("Y 오프셋:"));
    hbY->addWidget(spinOffsetY);
    setupLayout->addLayout(hbY);

    setupLayout->addStretch();
    mainLayout->addLayout(setupLayout);

    // Toolbar for Preview controls (Zoom, Pages, Print)
    QHBoxLayout *toolLayout = new QHBoxLayout();
    QPushButton *btnZoomIn = new QPushButton("🔍 확대 (+)");
    QPushButton *btnZoomOut = new QPushButton("🔍 축소 (-)");
    QPushButton *btnFitWidth = new QPushButton("↔ 너비 맞춤");
    QPushButton *btnFitPage = new QPushButton("📄 페이지 맞춤");
    QPushButton *btnPrev = new QPushButton("◀ 이전 페이지");
    QPushButton *btnNext = new QPushButton("다음 페이지 ▶");
    QPushButton *btnPrint = new QPushButton("🖨️ 인쇄하기 / PDF 저장...");
    btnPrint->setStyleSheet("background-color: #2563eb; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px;");
    QPushButton *btnClose = new QPushButton("닫기");

    toolLayout->addWidget(btnZoomIn);
    toolLayout->addWidget(btnZoomOut);
    toolLayout->addWidget(btnFitWidth);
    toolLayout->addWidget(btnFitPage);
    toolLayout->addSpacing(15);
    toolLayout->addWidget(btnPrev);
    toolLayout->addWidget(btnNext);
    toolLayout->addStretch();
    toolLayout->addWidget(btnPrint);
    toolLayout->addWidget(btnClose);
    mainLayout->addLayout(toolLayout);

    // Printer and Preview Widget
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageOrientation(QPageLayout::Portrait);
    QPrintPreviewWidget *previewWidget = new QPrintPreviewWidget(&printer, &previewDlg);
    mainLayout->addWidget(previewWidget, 1);

    // Initial PrintOptions
    PrintOptions options;
    if (view && view->selectionModel()->hasSelection() && view->selectionModel()->selectedIndexes().size() > 1) {
        radioSel->setChecked(true);
        options.rangeMode = PrintOptions::SelectedRange;
    } else {
        radioUsed->setChecked(true);
        options.rangeMode = PrintOptions::UsedRange;
    }

    auto updateCellEdits = [&]() {
        int minR, minC, maxR, maxC;
        SpreadsheetPrinter::getTargetRange(model, view, options, minR, minC, maxR, maxC);
        startEdit->blockSignals(true);
        endEdit->blockSignals(true);
        startEdit->setText(SpreadsheetPrinter::toCellRef(minR, minC));
        endEdit->setText(SpreadsheetPrinter::toCellRef(maxR, maxC));
        startEdit->blockSignals(false);
        endEdit->blockSignals(false);
    };

    updateCellEdits();

    connect(radioUsed, &QRadioButton::clicked, [&]() {
        options.rangeMode = PrintOptions::UsedRange;
        updateCellEdits();
        previewWidget->updatePreview();
    });
    connect(radioSel, &QRadioButton::clicked, [&]() {
        options.rangeMode = PrintOptions::SelectedRange;
        updateCellEdits();
        previewWidget->updatePreview();
    });
    connect(radioCustom, &QRadioButton::clicked, [&]() {
        options.rangeMode = PrintOptions::CustomRange;
        options.customStartCell = startEdit->text();
        options.customEndCell = endEdit->text();
        previewWidget->updatePreview();
    });
    connect(startEdit, &QLineEdit::textEdited, [&](const QString &txt) {
        radioCustom->setChecked(true);
        options.rangeMode = PrintOptions::CustomRange;
        options.customStartCell = txt;
        previewWidget->updatePreview();
    });
    connect(endEdit, &QLineEdit::textEdited, [&](const QString &txt) {
        radioCustom->setChecked(true);
        options.rangeMode = PrintOptions::CustomRange;
        options.customEndCell = txt;
        previewWidget->updatePreview();
    });
    connect(chkGrid, &QCheckBox::toggled, [&](bool checked) {
        options.printGridlines = checked;
        previewWidget->updatePreview();
    });
    connect(chkHead, &QCheckBox::toggled, [&](bool checked) {
        options.printHeaders = checked;
        previewWidget->updatePreview();
    });
    connect(chkFit, &QCheckBox::toggled, [&](bool checked) {
        options.fitToPageWidth = checked;
        previewWidget->updatePreview();
    });
    connect(chkCenterH, &QCheckBox::toggled, [&](bool checked) {
        options.centerHorizontal = checked;
        previewWidget->updatePreview();
    });
    connect(chkCenterV, &QCheckBox::toggled, [&](bool checked) {
        options.centerVertical = checked;
        previewWidget->updatePreview();
    });
    connect(spinOffsetX, QOverload<int>::of(&QSpinBox::valueChanged), [&](int value) {
        options.offsetX = value;
        previewWidget->updatePreview();
    });
    connect(spinOffsetY, QOverload<int>::of(&QSpinBox::valueChanged), [&](int value) {
        options.offsetY = value;
        previewWidget->updatePreview();
    });

    connect(btnZoomIn, &QPushButton::clicked, previewWidget, [previewWidget]() { previewWidget->zoomIn(); });
    connect(btnZoomOut, &QPushButton::clicked, previewWidget, [previewWidget]() { previewWidget->zoomOut(); });
    connect(btnFitWidth, &QPushButton::clicked, previewWidget, &QPrintPreviewWidget::fitToWidth);
    connect(btnFitPage, &QPushButton::clicked, previewWidget, &QPrintPreviewWidget::fitInView);
    connect(btnPrev, &QPushButton::clicked, previewWidget, [previewWidget]() {
        int page = previewWidget->currentPage() - 1;
        if (page >= 1) previewWidget->setCurrentPage(page);
    });
    connect(btnNext, &QPushButton::clicked, previewWidget, [previewWidget]() {
        int page = previewWidget->currentPage() + 1;
        if (page <= previewWidget->pageCount()) previewWidget->setCurrentPage(page);
    });
    connect(btnClose, &QPushButton::clicked, &previewDlg, &QDialog::accept);

    connect(btnPrint, &QPushButton::clicked, [&]() {
        QPrintDialog printDlg(&printer, &previewDlg);
        if (printDlg.exec() == QDialog::Accepted) {
            SpreadsheetPrinter::print(&printer, model, view, options);
            previewDlg.accept();
        }
    });

    connect(previewWidget, &QPrintPreviewWidget::paintRequested, [&](QPrinter *p) {
        SpreadsheetPrinter::print(p, model, view, options);
    });

    previewDlg.exec();
}

void MainWindow::applyBorders(const QString &type) {
    if (!m_tableView) return;
    SpreadsheetModel *model = currentModel();
    if (!model) return;
    QModelIndexList selList = m_tableView->selectionModel()->selectedIndexes();
    if (selList.isEmpty()) return;
    
    model->applyBorders(selList, type);
    m_tableView->viewport()->update();
}

void MainWindow::addNewFolder() {
    bool ok;
    QString name = QInputDialog::getText(this, "새 폴더", "폴더 이름을 입력하세요:", QLineEdit::Normal, "New Folder", &ok);
    if (ok && !name.isEmpty()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_sheetTree);
        item->setText(0, name);
        item->setIcon(0, QIcon()); // No specific icon or set a folder icon
        item->setData(0, Qt::UserRole, "folder");
        item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    }
}

void MainWindow::showTreeContextMenu(const QPoint &pos) {
    QTreeWidgetItem *item = m_sheetTree->itemAt(pos);
    if (!item) return;
    
    QMenu menu(this);
    if (item->data(0, Qt::UserRole).toString() == "folder") {
        QAction *renameAct = menu.addAction("이름 변경");
        QAction *delAct = menu.addAction("삭제");
        connect(renameAct, &QAction::triggered, this, [this, item]() {
            m_sheetTree->editItem(item);
        });
        connect(delAct, &QAction::triggered, this, [this, item]() {
            delete item;
        });
    } else {
        // It's a sheet
        int tabIdx = -1;
        for (int i=0; i<sheetTabs->count(); ++i) {
            if (sheetTabs->tabText(i) == item->text(0)) { tabIdx = i; break; }
        }
        if (tabIdx >= 0) {
            QAction *renameAct = menu.addAction("이름 변경");
            QAction *delAct = menu.addAction("삭제");
            QAction *dupAct = menu.addAction("복제");
            connect(renameAct, &QAction::triggered, this, [this, tabIdx]() { renameSheet(tabIdx); });
            connect(delAct, &QAction::triggered, this, [this, tabIdx]() { deleteSheet(tabIdx); });
            connect(dupAct, &QAction::triggered, this, [this, tabIdx]() { duplicateSheet(tabIdx); });
        }
    }
    menu.exec(m_sheetTree->mapToGlobal(pos));
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem *item, int column) {
    if (item->data(0, Qt::UserRole).toString() == "folder") return;
    QString sheetName = item->text(0);
    for (int i=0; i<sheetTabs->count(); ++i) {
        if (sheetTabs->tabText(i) == sheetName) {
            sheetTabs->setCurrentIndex(i);
            break;
        }
    }
}

void MainWindow::onTreeItemDoubleClicked(QTreeWidgetItem *item, int column) {
    if (item->data(0, Qt::UserRole).toString() == "folder") {
        m_sheetTree->editItem(item);
    } else {
        int tabIdx = -1;
        for (int i=0; i<sheetTabs->count(); ++i) {
            if (sheetTabs->tabText(i) == item->text(0)) { tabIdx = i; break; }
        }
        if (tabIdx >= 0) renameSheet(tabIdx);
    }
}

void MainWindow::populateTreeFromTabs() {
    m_sheetTree->clear();
    for (int i=0; i<sheetTabs->count(); ++i) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_sheetTree);
        item->setText(0, sheetTabs->tabText(i));
        item->setData(0, Qt::UserRole, "sheet");
        item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
        item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled); // Sheets shouldn't be droppable into
    }
}

void MainWindow::syncTreeToTabs() {
    // A simple sync that adds missing sheets to the tree. 
    // It doesn't rebuild to preserve folders.
    QSet<QString> treeSheets;
    
    QTreeWidgetItemIterator it(m_sheetTree);
    while (*it) {
        if ((*it)->data(0, Qt::UserRole).toString() == "sheet") {
            treeSheets.insert((*it)->text(0));
            // Ensure sheet items can't accept drops
            (*it)->setFlags((*it)->flags() & ~Qt::ItemIsDropEnabled);
            // Change rename behavior: let renameSheet handle it instead of inline editing
            (*it)->setFlags((*it)->flags() & ~Qt::ItemIsEditable);
        } else {
            (*it)->setFlags((*it)->flags() | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);
        }
        ++it;
    }
    
    for (int i=0; i<sheetTabs->count(); ++i) {
        if (!treeSheets.contains(sheetTabs->tabText(i))) {
            QTreeWidgetItem *item = new QTreeWidgetItem(m_sheetTree);
            item->setText(0, sheetTabs->tabText(i));
            item->setData(0, Qt::UserRole, "sheet");
            item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
            item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled & ~Qt::ItemIsEditable);
        }
    }
    
    // Remove deleted sheets
    QSet<QString> tabSheets;
    for (int i=0; i<sheetTabs->count(); ++i) tabSheets.insert(sheetTabs->tabText(i));
    QTreeWidgetItemIterator it2(m_sheetTree);
    while (*it2) {
        QTreeWidgetItem *item = *it2;
        ++it2;
        if (item->data(0, Qt::UserRole).toString() == "sheet" && !tabSheets.contains(item->text(0))) {
            delete item;
        }
    }
    
    // Update selection
    if (sheetTabs->currentIndex() >= 0) {
        QString currentName = sheetTabs->tabText(sheetTabs->currentIndex());
        QTreeWidgetItemIterator it3(m_sheetTree);
        while (*it3) {
            if ((*it3)->data(0, Qt::UserRole).toString() == "sheet" && (*it3)->text(0) == currentName) {
                m_sheetTree->setCurrentItem(*it3);
                break;
            }
            ++it3;
        }
    }
}

QJsonArray MainWindow::serializeTree() const {
    QJsonArray treeArray;
    for (int i=0; i<m_sheetTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_sheetTree->topLevelItem(i);
        QJsonObject obj;
        obj["name"] = item->text(0);
        obj["type"] = item->data(0, Qt::UserRole).toString();
        if (obj["type"].toString() == "folder") {
            obj["expanded"] = item->isExpanded();
            QJsonArray children;
            for (int j=0; j<item->childCount(); ++j) {
                QTreeWidgetItem *child = item->child(j);
                QJsonObject cobj;
                cobj["name"] = child->text(0);
                cobj["type"] = child->data(0, Qt::UserRole).toString();
                children.append(cobj);
            }
            obj["children"] = children;
        }
        treeArray.append(obj);
    }
    return treeArray;
}

void MainWindow::buildTreeFromJson(const QJsonArray &treeArray) {
    m_sheetTree->clear();
    for (int i=0; i<treeArray.size(); ++i) {
        QJsonObject obj = treeArray[i].toObject();
        QTreeWidgetItem *item = new QTreeWidgetItem(m_sheetTree);
        item->setText(0, obj["name"].toString());
        QString type = obj["type"].toString();
        item->setData(0, Qt::UserRole, type);
        
        if (type == "folder") {
            item->setIcon(0, QIcon()); 
            item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
            if (obj.contains("expanded") && obj["expanded"].toBool()) {
                m_sheetTree->expandItem(item);
            }
            
            QJsonArray children = obj["children"].toArray();
            for (int j=0; j<children.size(); ++j) {
                QJsonObject cobj = children[j].toObject();
                QTreeWidgetItem *child = new QTreeWidgetItem(item);
                child->setText(0, cobj["name"].toString());
                child->setData(0, Qt::UserRole, cobj["type"].toString());
                child->setFlags(child->flags() | Qt::ItemIsDragEnabled);
                child->setFlags(child->flags() & ~Qt::ItemIsDropEnabled & ~Qt::ItemIsEditable);
            }
        } else {
            item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
            item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled & ~Qt::ItemIsEditable);
        }
    }
}

#include "CustomTableView.h"
#include <QMouseEvent>
#include <QPainter>
#include "SpreadsheetModel.h"
#include "SpreadsheetPrinter.h"
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QGuiApplication>
#include <QLineEdit>
#include <QTabWidget>
#include "MainWindow.h"
#include <QPlainTextEdit>

CustomTableView::CustomTableView(QWidget *parent) : QTableView(parent), m_tabStartCol(-1) {
    setItemDelegate(new SpreadsheetDelegate(this));
    
    // 헤더 클릭 시 해당 영역 전체 선택 (요구사항 3번)
    setSelectionBehavior(QAbstractItemView::SelectItems);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    if (horizontalHeader()) horizontalHeader()->setSectionsClickable(true);
    if (verticalHeader()) verticalHeader()->setSectionsClickable(true);
    
    connect(horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int logicalIndex) {
        selectColumn(logicalIndex);
    });
    connect(verticalHeader(), &QHeaderView::sectionClicked, this, [this](int logicalIndex) {
        selectRow(logicalIndex);
    });
}

void CustomTableView::paintEvent(QPaintEvent *event) {
    QTableView::paintEvent(event);

    QPainter painter(this->viewport());
    SpreadsheetModel *m = qobject_cast<SpreadsheetModel*>(model());
    if (!m) return;

    painter.save();
    painter.setClipRect(event->rect());

    QHash<QPair<int, int>, int> borders = m->getBorders();
    for (auto it = borders.begin(); it != borders.end(); ++it) {
        int r = it.key().first;
        int c = it.key().second;
        int b = it.value();
        if (b == 0) continue;

        QModelIndex idx = m->index(r, c);
        if (!idx.isValid()) continue;

        QRect rect = visualRect(idx);
        if (!rect.intersects(event->rect())) continue;

        int top = (b >> 0) & 0xF;
        int bottom = (b >> 4) & 0xF;
        int left = (b >> 8) & 0xF;
        int right = (b >> 12) & 0xF;

        auto drawEdge = [&](int x1, int y1, int x2, int y2, int style, bool isHoriz) {
            if (style == 0) return;
            QPen pen(Qt::black);
            if (style == 3) {
                pen.setWidth(1);
                painter.setPen(pen);
                painter.drawLine(x1, y1, x2, y2);
                if (isHoriz) {
                    int offset = (y1 == rect.top()) ? 2 : -2;
                    painter.drawLine(x1, y1 + offset, x2, y2 + offset);
                } else {
                    int offset = (x1 == rect.left()) ? 2 : -2;
                    painter.drawLine(x1 + offset, y1, x2 + offset, y2);
                }
            } else {
                pen.setWidth(style == 2 ? 3 : 1);
                painter.setPen(pen);
                painter.drawLine(x1, y1, x2, y2);
            }
        };

        if (top) drawEdge(rect.left(), rect.top(), rect.right(), rect.top(), top, true);
        if (bottom) drawEdge(rect.left(), rect.bottom(), rect.right(), rect.bottom(), bottom, true);
        if (left) drawEdge(rect.left(), rect.top(), rect.left(), rect.bottom(), left, false);
        if (right) drawEdge(rect.right(), rect.top(), rect.right(), rect.bottom(), right, false);
    }
    painter.restore();
}

void CustomTableView::commitCurrentEditor() {
    if (state() == QAbstractItemView::EditingState) {
        QWidget *ed = indexWidget(currentIndex());
        if (!ed) ed = focusWidget();
        if (ed) {
            commitData(ed);
            closeEditor(ed, QAbstractItemDelegate::NoHint);
        }
    }
}

void CustomTableView::closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint) {
    if (hint == QAbstractItemDelegate::SubmitModelCache) { // 편집 중 Enter 누름
        QTableView::closeEditor(editor, hint); // 기본 동작(아래로 이동) 수행 허용
        
        int nextCol = (m_tabStartCol != -1) ? m_tabStartCol : currentIndex().column();
        Qt::KeyboardModifiers mods = QGuiApplication::keyboardModifiers();
        
        // Qt의 내부 처리가 끝난 뒤 커서를 원하는 곳으로 강제 이동
        QMetaObject::invokeMethod(this, [this, nextCol, mods]() {
            int dr = (mods & Qt::ShiftModifier) ? -1 : 1;
            int currentRow = currentIndex().row() + dr; 
            if (currentRow >= 0 && currentRow < model()->rowCount()) {
                QModelIndex nextIndex = model()->index(currentRow, nextCol);
                setCurrentIndex(nextIndex);
            }
            m_tabStartCol = -1; // 타자기 복귀점 초기화
        }, Qt::QueuedConnection);
        return;
    }
    else if (hint == QAbstractItemDelegate::EditNextItem) { // 편집 중 Tab 누름
        if (m_tabStartCol == -1) {
            m_tabStartCol = currentIndex().column();
        }
        QTableView::closeEditor(editor, hint);
        return;
    }
    QTableView::closeEditor(editor, hint);
}

void CustomTableView::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
    QTableView::currentChanged(current, previous);
    if (current.isValid()) {
        SpreadsheetModel *sm = qobject_cast<SpreadsheetModel*>(model());
        if (sm) {
            sm->setCursorPos(current.row(), current.column());
        }
        if (!(QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)) {
            m_selectionAnchor = current;
        }
    }
}

QModelIndex CustomTableView::findCtrlArrowTarget(const QModelIndex &start, int dr, int dc) {
    if (!start.isValid()) return start;
    int r = start.row();
    int c = start.column();
    int maxR = model()->rowCount() - 1;
    int maxC = model()->columnCount() - 1;

    auto isEmpty = [this](int row, int col) -> bool {
        if (row < 0 || row >= model()->rowCount() || col < 0 || col >= model()->columnCount()) return true;
        return model()->index(row, col).data(Qt::DisplayRole).toString().isEmpty();
    };

    int nextR = r + dr;
    int nextC = c + dc;
    if (nextR < 0 || nextR > maxR || nextC < 0 || nextC > maxC) {
        return start;
    }

    bool startEmpty = isEmpty(r, c);
    bool nextEmpty = isEmpty(nextR, nextC);

    int currR = nextR;
    int currC = nextC;

    if (startEmpty || nextEmpty) {
        while (currR >= 0 && currR <= maxR && currC >= 0 && currC <= maxC) {
            if (!isEmpty(currR, currC)) {
                return model()->index(currR, currC);
            }
            currR += dr;
            currC += dc;
        }
        currR -= dr;
        currC -= dc;
        return model()->index(qBound(0, currR, maxR), qBound(0, currC, maxC));
    } else {
        while (currR >= 0 && currR <= maxR && currC >= 0 && currC <= maxC) {
            int checkR = currR + dr;
            int checkC = currC + dc;
            if (checkR < 0 || checkR > maxR || checkC < 0 || checkC > maxC || isEmpty(checkR, checkC)) {
                return model()->index(currR, currC);
            }
            currR = checkR;
            currC = checkC;
        }
        return model()->index(qBound(0, currR, maxR), qBound(0, currC, maxC));
    }
}

void CustomTableView::restoreMergedRanges() {
    clearSpans();
    SpreadsheetModel *sm = qobject_cast<SpreadsheetModel*>(model());
    if (sm) {
        for (const QRect &r : sm->getMergedRanges()) {
            setSpan(r.y(), r.x(), r.height(), r.width());
        }
    }
}

void CustomTableView::keyPressEvent(QKeyEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab || 
            event->key() == Qt::Key_PageUp || event->key() == Qt::Key_PageDown) {
            event->ignore();
            return;
        }
        if (event->key() == Qt::Key_Z || event->key() == Qt::Key_Y) {
            if (state() != QAbstractItemView::EditingState) {
                SpreadsheetModel *sm = qobject_cast<SpreadsheetModel*>(model());
                if (sm) {
                    if ((event->key() == Qt::Key_Z && (event->modifiers() & Qt::ShiftModifier)) || event->key() == Qt::Key_Y) {
                        sm->redo();
                    } else {
                        sm->undo();
                    }
                    restoreMergedRanges();
                    QPair<int, int> pos = sm->getCursorPos();
                    if (pos.first >= 0 && pos.second >= 0 && pos.first < sm->rowCount() && pos.second < sm->columnCount()) {
                        setCurrentIndex(sm->index(pos.first, pos.second));
                    }
                    viewport()->update();
                    update();
                }
                return;
            } else {
                QTableView::keyPressEvent(event);
                return;
            }
        }
        if (event->key() == Qt::Key_D) {
            if (state() != QAbstractItemView::EditingState) {
                SpreadsheetModel *sm = qobject_cast<SpreadsheetModel*>(model());
                if (sm) {
                    sm->applyFillDown(selectionModel()->selectedIndexes());
                    viewport()->update();
                    update();
                }
                return;
            }
        }
    }

    bool isArrowKey = (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right || 
                       event->key() == Qt::Key_Up || event->key() == Qt::Key_Down);
    bool isFullRow = !selectionModel()->selectedRows().isEmpty();
    bool isFullCol = !selectionModel()->selectedColumns().isEmpty();

    if (isArrowKey && state() != QAbstractItemView::EditingState) {
        if ((event->modifiers() & Qt::ControlModifier) || 
            ((event->modifiers() & Qt::ShiftModifier) && (isFullRow || isFullCol))) {
            
            int dr = 0, dc = 0;
            if (event->key() == Qt::Key_Left) dc = -1;
            else if (event->key() == Qt::Key_Right) dc = 1;
            else if (event->key() == Qt::Key_Up) dr = -1;
            else if (event->key() == Qt::Key_Down) dr = 1;

            QModelIndex target;
            if ((isFullRow && dr != 0) || (isFullCol && dc != 0)) {
                target = model()->index(currentIndex().row() + dr, currentIndex().column() + dc);
            } else {
                target = findCtrlArrowTarget(currentIndex(), dr, dc);
            }

            if (target.isValid() && target != currentIndex()) {
                if ((event->modifiers() & Qt::ShiftModifier) || ((isFullRow || isFullCol) && (event->modifiers() & Qt::ControlModifier))) {
                    if (!m_selectionAnchor.isValid()) m_selectionAnchor = currentIndex();
                    int minR = qMin(m_selectionAnchor.row(), target.row());
                    int maxR = qMax(m_selectionAnchor.row(), target.row());
                    int minC = qMin(m_selectionAnchor.column(), target.column());
                    int maxC = qMax(m_selectionAnchor.column(), target.column());
                    
                    if (isFullRow && dr != 0) {
                        minC = 0; maxC = model()->columnCount() - 1;
                    } else if (isFullCol && dc != 0) {
                        minR = 0; maxR = model()->rowCount() - 1;
                    }

                    QItemSelection sel(model()->index(minR, minC), model()->index(maxR, maxC));
                    selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
                    selectionModel()->setCurrentIndex(target, QItemSelectionModel::NoUpdate);
                } else {
                    selectionModel()->setCurrentIndex(target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
                    m_selectionAnchor = target;
                }
                scrollTo(target);
            }
            event->accept();
            return;
        }
    }

    if (event->key() == Qt::Key_Delete) {
        QModelIndexList selected = selectionModel()->selectedIndexes();
        SpreadsheetModel *sm = qobject_cast<SpreadsheetModel*>(model());
        if (sm) sm->beginMacro();
        for (const QModelIndex &idx : selected) {
            model()->setData(idx, "", Qt::EditRole);
        }
        if (sm) sm->endMacro();
        return;
    }

    if (event->key() == Qt::Key_Tab) {
        if (m_tabStartCol == -1) {
            m_tabStartCol = currentIndex().column(); // 탭 이동 시작점 기억
        }
        QTableView::keyPressEvent(event); // 기본 동작(우측 이동) 수행
        return;
    } 
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (MainWindow::s_activeFormulaEditor) {
            // Commit the cross-sheet formula editor
            QKeyEvent *enterEvent = new QKeyEvent(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
            QCoreApplication::postEvent(MainWindow::s_activeFormulaEditor, enterEvent);
            
            // Switch back to the starting sheet
            QTabWidget *tabs = nullptr;
            if (parentWidget() && parentWidget()->parentWidget()) {
                tabs = qobject_cast<QTabWidget*>(parentWidget()->parentWidget());
            }
            if (tabs && !MainWindow::s_formulaStartSheet.isEmpty()) {
                for (int i = 0; i < tabs->count(); ++i) {
                    if (tabs->tabText(i) == MainWindow::s_formulaStartSheet) {
                        tabs->setCurrentIndex(i);
                        break;
                    }
                }
            }
            MainWindow::s_activeFormulaEditor = nullptr;
            MainWindow::s_formulaStartSheet = "";
            MainWindow::s_isSelectingFormulaRange = false;
            return;
        }

        if (state() != QAbstractItemView::EditingState) {
            int nextCol = (m_tabStartCol != -1) ? m_tabStartCol : currentIndex().column();
            int nextRow = currentIndex().row() + 1;
            
            if (nextRow < model()->rowCount()) {
                QModelIndex nextIndex = model()->index(nextRow, nextCol);
                setCurrentIndex(nextIndex);
            }
            
            m_tabStartCol = -1; // 타자기 리턴 완료 후 시작점 초기화
            return;
        } else {
            QTableView::keyPressEvent(event);
            return;
        }
    }
    else {
        // 방향키 등 다른 키 입력 시 탭 이동 기록 초기화
        if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right || 
            event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
            m_tabStartCol = -1;
        }
        QTableView::keyPressEvent(event);
    }
}

void CustomTableView::mousePressEvent(QMouseEvent *event) {
    QModelIndex index = indexAt(event->pos());
    
    // 수식 입력 중 타 셀 클릭 가로채기
    if (MainWindow::s_activeFormulaEditor && index.isValid()) {
        QTabWidget *tabs = nullptr;
        if (parentWidget() && parentWidget()->parentWidget()) {
            tabs = qobject_cast<QTabWidget*>(parentWidget()->parentWidget());
        }
        
        QString cell = SpreadsheetPrinter::toCellRef(index.row(), index.column());
        if (tabs) {
            QString currentSheet = tabs->tabText(tabs->indexOf(this));
            if (currentSheet != MainWindow::s_formulaStartSheet && !MainWindow::s_formulaStartSheet.isEmpty()) {
                QString sheetPrefix = currentSheet;
                QRegularExpression rx("[^A-Za-z0-9_]");
                if (rx.match(sheetPrefix).hasMatch()) {
                    sheetPrefix = "'" + sheetPrefix + "'";
                }
                cell = sheetPrefix + "!" + cell;
            }
        }
        
        QString txt;
        if (auto le = qobject_cast<QLineEdit*>(MainWindow::s_activeFormulaEditor)) txt = le->text();
        else if (auto pe = qobject_cast<QPlainTextEdit*>(MainWindow::s_activeFormulaEditor)) txt = pe->toPlainText();
        
        MainWindow::s_formulaTextBeforeDrag = txt;
        
        QString newTxt = txt + cell;
        if (auto le = qobject_cast<QLineEdit*>(MainWindow::s_activeFormulaEditor)) le->setText(newTxt);
        else if (auto pe = qobject_cast<QPlainTextEdit*>(MainWindow::s_activeFormulaEditor)) pe->setPlainText(newTxt);
        
        MainWindow::s_isSelectingFormulaRange = true;
        m_selectionAnchor = index; // 드래그 시작점
        
        // 포커스는 유지
        MainWindow::s_activeFormulaEditor->setFocus();
        return; // 셀 선택 방지
    }

    if (index.isValid()) {
        SpreadsheetModel *m = qobject_cast<SpreadsheetModel*>(model());
        if (m && m->isFilterHeader(index.row(), index.column())) {
            // 화살표 아이콘(우측 16px) 클릭 여부 확인
            QRect rect = visualRect(index);
            if (event->pos().x() > rect.right() - 20) {
                QMenu menu(this);
                QAction *asc = menu.addAction("오름차순 정렬 (Sort A-Z)");
                QAction *desc = menu.addAction("내림차순 정렬 (Sort Z-A)");
                
                QAction *res = menu.exec(event->globalPosition().toPoint());
                if (res == asc) {
                    m->sortRange(index.column(), Qt::AscendingOrder);
                } else if (res == desc) {
                    m->sortRange(index.column(), Qt::DescendingOrder);
                }
                return; // 이벤트 소모 (셀 선택 방지)
            }
        }
    }
    QTableView::mousePressEvent(event);
}

void CustomTableView::mouseMoveEvent(QMouseEvent *event) {
    if (MainWindow::s_activeFormulaEditor && MainWindow::s_isSelectingFormulaRange) {
        QModelIndex index = indexAt(event->pos());
        if (index.isValid() && m_selectionAnchor.isValid() && index != m_selectionAnchor) {
            int minR = qMin(m_selectionAnchor.row(), index.row());
            int maxR = qMax(m_selectionAnchor.row(), index.row());
            int minC = qMin(m_selectionAnchor.column(), index.column());
            int maxC = qMax(m_selectionAnchor.column(), index.column());
            
            QTabWidget *tabs = nullptr;
            if (parentWidget() && parentWidget()->parentWidget()) {
                tabs = qobject_cast<QTabWidget*>(parentWidget()->parentWidget());
            }
            
            QString rangeRef = SpreadsheetPrinter::toCellRef(minR, minC) + ":" + SpreadsheetPrinter::toCellRef(maxR, maxC);
            if (tabs) {
                QString currentSheet = tabs->tabText(tabs->indexOf(this));
                if (currentSheet != MainWindow::s_formulaStartSheet && !MainWindow::s_formulaStartSheet.isEmpty()) {
                    QString sheetPrefix = currentSheet;
                    QRegularExpression rx("[^A-Za-z0-9_]");
                    if (rx.match(sheetPrefix).hasMatch()) {
                        sheetPrefix = "'" + sheetPrefix + "'";
                    }
                    rangeRef = sheetPrefix + "!" + rangeRef;
                }
            }
            
            QString newTxt = MainWindow::s_formulaTextBeforeDrag + rangeRef;
            if (auto le = qobject_cast<QLineEdit*>(MainWindow::s_activeFormulaEditor)) le->setText(newTxt);
            else if (auto pe = qobject_cast<QPlainTextEdit*>(MainWindow::s_activeFormulaEditor)) pe->setPlainText(newTxt);
        }
        return;
    }
    QTableView::mouseMoveEvent(event);
}

void CustomTableView::mouseReleaseEvent(QMouseEvent *event) {
    if (MainWindow::s_activeFormulaEditor && MainWindow::s_isSelectingFormulaRange) {
        MainWindow::s_isSelectingFormulaRange = false;
        MainWindow::s_activeFormulaEditor->setFocus();
        return;
    }
    QTableView::mouseReleaseEvent(event);
}

SpreadsheetDelegate::SpreadsheetDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void SpreadsheetDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const {
    QStyledItemDelegate::initStyleOption(option, index);
    if (index.data(Qt::UserRole + 4).toBool()) {
        option->text = QString();
    }
}

void SpreadsheetDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QStyledItemDelegate::paint(painter, option, index);
    
    bool isVert = index.data(Qt::UserRole + 4).toBool();
    QString text = index.data(Qt::DisplayRole).toString();
    
    if (isVert && !text.isEmpty()) {
        painter->save();
        QVariant fontVar = index.data(Qt::FontRole);
        QFont font = fontVar.isValid() ? fontVar.value<QFont>() : option.font;
        painter->setFont(font);
        
        QColor fgColor = index.data(Qt::ForegroundRole).value<QColor>();
        if (fgColor.isValid()) {
            painter->setPen(fgColor);
        } else {
            painter->setPen(option.palette.color(QPalette::Text));
        }

        QFontMetrics fm(font);
        int align = index.data(Qt::TextAlignmentRole).toInt();
        
        // 여러 줄(수동 줄바꿈) 지원을 위해 \n으로 분리
        QStringList lines = text.split('\n');
        
        // 줄 하나당 폭(가장 넓은 글자 기준이거나 고정 폭)
        int colWidth = fm.height(); 
        int totalWidth = colWidth * lines.size();
        
        // 전체 줄들이 그려질 시작 X 위치 계산 (우측에서 좌측으로 쓰는 것이 일반적)
        int startX = option.rect.right() - colWidth;
        if (align & Qt::AlignHCenter) {
            startX = option.rect.center().x() + (totalWidth / 2) - colWidth;
        } else if (align & Qt::AlignLeft) {
            startX = option.rect.left() + totalWidth - colWidth;
        }
        
        // 여백 약간
        int margin = 2;
        
        for (int l = 0; l < lines.size(); ++l) {
            QString line = lines[l];
            int totalHeight = fm.height() * line.length();
            
            int y = option.rect.top() + margin;
            if (align & Qt::AlignVCenter) {
                y += qMax(0, (option.rect.height() - totalHeight) / 2);
            } else if (align & Qt::AlignBottom) {
                y = option.rect.bottom() - totalHeight - margin;
            }
            y += fm.ascent();
            
            int x = startX - (l * colWidth);
            
            for (int i = 0; i < line.length(); ++i) {
                QChar c = line[i];
                int charWidth = fm.horizontalAdvance(c);
                painter->drawText(x + (colWidth - charWidth)/2, y, QString(c));
                y += fm.height();
            }
        }
        painter->restore();
    }
    
    // 필터 헤더인 경우 드롭다운 화살표(▼) 아이콘 렌더링
    const SpreadsheetModel *m = qobject_cast<const SpreadsheetModel*>(index.model());
    if (m && m->isFilterHeader(index.row(), index.column())) {
        QRect rect = option.rect;
        QRect buttonRect(rect.right() - 18, rect.top() + (rect.height() - 16)/2, 16, 16);
        painter->fillRect(buttonRect, Qt::lightGray);
        painter->setPen(Qt::black);
        painter->drawText(buttonRect, Qt::AlignCenter, "▼");
    }

    // 기본 회색 격자 렌더링 (CustomTableView에서 기본 격자를 끄고 여기서 직접 그림)
    painter->save();
    painter->setPen(QColor(212, 212, 212)); // 기본 Qt 격자색
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    painter->drawLine(option.rect.topRight(), option.rect.bottomRight());
    painter->restore();

}

QWidget *SpreadsheetDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QPlainTextEdit *editor = new QPlainTextEdit(parent);
    editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor->setStyleSheet("QPlainTextEdit { border: none; padding: 0px; background-color: white; }");
    editor->installEventFilter(const_cast<SpreadsheetDelegate*>(this));
    return editor;
}

void SpreadsheetDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    if (QPlainTextEdit *pe = qobject_cast<QPlainTextEdit*>(editor)) {
        pe->setPlainText(index.data(Qt::EditRole).toString());
    } else {
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

void SpreadsheetDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
    if (QPlainTextEdit *pe = qobject_cast<QPlainTextEdit*>(editor)) {
        model->setData(index, pe->toPlainText(), Qt::EditRole);
    } else {
        QStyledItemDelegate::setModelData(editor, model, index);
    }
}

bool SpreadsheetDelegate::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::FocusOut) {
        if (QPlainTextEdit *pe = qobject_cast<QPlainTextEdit*>(object)) {
            QString text = pe->toPlainText();
            if (text.startsWith("=")) {
                // 수식 편집 중이므로 에디터를 닫지 않고 상태 유지
                if (!MainWindow::s_activeFormulaEditor) {
                    MainWindow::s_activeFormulaEditor = pe;
                    QTableView *view = qobject_cast<QTableView*>(pe->parentWidget());
                    if (view && view->parentWidget() && view->parentWidget()->parentWidget()) {
                        QTabWidget *tabs = qobject_cast<QTabWidget*>(view->parentWidget()->parentWidget());
                        if (tabs) MainWindow::s_formulaStartSheet = tabs->tabText(tabs->currentIndex());
                    }
                }
                return true; // FocusOut 무시
            }
        }
    } else if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) {
            if (keyEvent->modifiers() & Qt::AltModifier) {
                // Alt+Enter: 수동 줄바꿈 삽입
                if (QPlainTextEdit *pe = qobject_cast<QPlainTextEdit*>(object)) {
                    pe->insertPlainText("\n");
                    return true;
                }
            } else if (keyEvent->modifiers() == Qt::NoModifier || keyEvent->modifiers() == Qt::ShiftModifier) {
                // Enter: 편집 완료 및 다음 셀로 이동 (Shift+Enter는 위로 이동)
                MainWindow::s_activeFormulaEditor = nullptr;
                MainWindow::s_formulaStartSheet = "";
                MainWindow::s_isSelectingFormulaRange = false;
                
                if (QPlainTextEdit *pe = qobject_cast<QPlainTextEdit*>(object)) {
                    emit commitData(pe);
                    emit closeEditor(pe, QAbstractItemDelegate::SubmitModelCache);
                    return true;
                }
            }
        } else if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
            MainWindow::s_activeFormulaEditor = nullptr;
            MainWindow::s_formulaStartSheet = "";
            MainWindow::s_isSelectingFormulaRange = false;
            
            if (QPlainTextEdit *pe = qobject_cast<QPlainTextEdit*>(object)) {
                emit commitData(pe);
                if (keyEvent->modifiers() & Qt::ShiftModifier) {
                    emit closeEditor(pe, QAbstractItemDelegate::EditPreviousItem);
                } else {
                    emit closeEditor(pe, QAbstractItemDelegate::EditNextItem);
                }
                return true;
            }
        }
    }
    return QStyledItemDelegate::eventFilter(object, event);
}

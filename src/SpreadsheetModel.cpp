#include "SpreadsheetModel.h"
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QRegularExpression>
#include <QColor>
#include "MainWindow.h"
#include <QFileInfo>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamWriter>
#include <QTemporaryFile>

SpreadsheetModel::SpreadsheetModel(QObject *parent) 
    : QAbstractTableModel(parent), m_file(nullptr), m_columnCount(16384), 
      m_filterHeaderRow(-1), m_filterStartCol(-1), m_filterEndCol(-1) {
    
    // CSV 로드 없이도 자체 100행 그리드에서 정렬이 동작하도록 초기화
    for (int i = 0; i < 100; ++i) {
        m_rowMap.push_back(i);
    }
    m_rangeRowMap = m_rowMap;
}

SpreadsheetModel::~SpreadsheetModel() {
    if (m_file) {
        m_file->close();
        delete m_file;
    }
}

bool SpreadsheetModel::loadCsv(const QString &filePath) {
    beginResetModel();
    if (m_file) { m_file->close(); delete m_file; }
    m_file = new QFile(filePath, this);
    if (!m_file->open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_file->deleteLater(); m_file = nullptr; endResetModel(); return false;
    }
    m_lineOffsets.clear(); m_lineOffsets.push_back(0);
    qint64 offset = 0; char buffer[8192]; qint64 bytesRead;
    while ((bytesRead = m_file->read(buffer, sizeof(buffer))) > 0) {
        for (qint64 i = 0; i < bytesRead; ++i) {
            if (buffer[i] == '\n') m_lineOffsets.push_back(offset + i + 1);
        }
        offset += bytesRead;
    }
    if (!m_lineOffsets.empty() && m_lineOffsets.back() >= m_file->size()) m_lineOffsets.pop_back();
    if (!m_lineOffsets.empty()) {
        QString firstLine = getCellData(0, -1);
        int cols = firstLine.split(',').size();
        m_columnCount = qMax(16384, cols);
    }
    
    // 행 매핑 인덱스 초기화 (정렬용)
    m_rowMap.clear();
    for (int i = 0; i < (int)m_lineOffsets.size(); ++i) {
        m_rowMap.push_back(i);
    }
    m_rangeRowMap = m_rowMap; // 초기에는 동일
    
    m_editedData.clear(); m_fonts.clear();
    m_filterHeaderRow = -1;
    endResetModel();
    clearUndoStack();
    return true;
}

int SpreadsheetModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_rowMap.size()); // CSV 유무에 상관없이 매핑 배열 크기 반환
}

int SpreadsheetModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_columnCount;
}

QString SpreadsheetModel::getCellData(int logicalRow, int col) const {
    if (!m_file || !m_file->isOpen()) return "";
    if (logicalRow < 0 || logicalRow >= static_cast<int>(m_lineOffsets.size())) return "";
    m_file->seek(m_lineOffsets[logicalRow]);
    QString line = QString::fromUtf8(m_file->readLine()).trimmed();
    if (col == -1) return line;
    QStringList parts = line.split(',');
    if (col < parts.size()) return parts[col];
    return "";
}

// 화면에 보이는 Visual Row를 Logical Row로 매핑하여 값을 읽어옵니다
QString SpreadsheetModel::getRawCellValue(int visualRow, int col) const {
    int logicalRow = visualRow;
    if (visualRow >= 0 && visualRow < (int)m_rowMap.size()) {
        logicalRow = m_rowMap[visualRow];
    }
    QPair<int, int> pos = qMakePair(logicalRow, col);
    if (m_editedData.contains(pos)) return m_editedData[pos];
    return getCellData(logicalRow, col);
}

Qt::ItemFlags SpreadsheetModel::flags(const QModelIndex &index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

bool SpreadsheetModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid()) return false;
    if (role == Qt::EditRole || role == Qt::DisplayRole) {
        int logicalRow = m_rowMap.empty() ? index.row() : m_rowMap[index.row()];
        pushUndo();
        m_editedData[qMakePair(logicalRow, index.column())] = value.toString();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    }
    return false;
}

int SpreadsheetModel::colFromLetters(const QString &letters) const {
    int col = 0;
    for (int i = 0; i < letters.length(); ++i) {
        col = col * 26 + (letters[i].toUpper().unicode() - 'A' + 1);
    }
    return col - 1;
}

namespace MathExpr {
    class Parser {
        QString s;
        int pos;
        QChar peek() {
            while (pos < s.length() && s[pos].isSpace()) pos++;
            return pos < s.length() ? s[pos] : QChar();
        }
        QChar get() {
            QChar c = peek();
            if (!c.isNull()) pos++;
            return c;
        }
        double parsePrimary() {
            QChar c = peek();
            if (c == '(') {
                get();
                double v = parseExpr();
                if (peek() == ')') get();
                return v;
            }
            if (c == '-' || c == '+') {
                QChar op = get();
                double v = parsePrimary();
                return op == '-' ? -v : v;
            }
            int start = pos;
            while (pos < s.length() && (s[pos].isDigit() || s[pos] == '.')) {
                pos++;
            }
            if (start == pos) return 0.0;
            return s.mid(start, pos - start).toDouble();
        }
        double parseTerm() {
            double v = parsePrimary();
            while (true) {
                QChar c = peek();
                if (c == '*' || c == '/') {
                    get();
                    double v2 = parsePrimary();
                    if (c == '*') v *= v2;
                    else if (v2 != 0) v /= v2;
                } else {
                    break;
                }
            }
            return v;
        }
        double parseExpr() {
            double v = parseTerm();
            while (true) {
                QChar c = peek();
                if (c == '+' || c == '-') {
                    get();
                    double v2 = parseTerm();
                    if (c == '+') v += v2;
                    else v -= v2;
                } else {
                    break;
                }
            }
            return v;
        }
    public:
        double evaluate(const QString &expr) {
            s = expr;
            pos = 0;
            return parseExpr();
        }
    };
}

QString SpreadsheetModel::evaluateFormula(const QString &formula) const {
    static int recursionDepth = 0;
    if (recursionDepth > 1000) return "#REF!";
    recursionDepth++;

    auto getStringValue = [this](const QString &sheetName, int r, int c) -> QString {
        if (sheetName.isEmpty()) {
            return getRawCellValue(r, c);
        } else {
            MainWindow *mw = qobject_cast<MainWindow*>(parent());
            if (mw) {
                SpreadsheetModel *sm = mw->getSheetModel(sheetName);
                if (sm) {
                    return sm->data(sm->index(r, c), Qt::UserRole + 10).toString();
                }
            }
            return "";
        }
    };

    auto getNumValue = [&](const QString &sheetName, int r, int c) -> double {
        if (sheetName.isEmpty()) {
            QString val = getRawCellValue(r, c);
            if (val.startsWith("=")) {
                return evaluateFormula(val.mid(1)).toDouble();
            }
            return val.toDouble();
        } else {
            MainWindow *mw = qobject_cast<MainWindow*>(parent());
            if (mw) {
                SpreadsheetModel *sm = mw->getSheetModel(sheetName);
                if (sm) {
                    return sm->data(sm->index(r, c), Qt::UserRole + 10).toDouble();
                }
            }
            return 0.0;
        }
    };

    // 1. 단일 셀 참조인지 확인 (단일 텍스트 셀인 경우 문자열 반환을 위함)
    QRegularExpression exactCellRegex("^(?:'?([^!']+)'?!)?([A-Z]+)(\\d+)$", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch exactCellMatch = exactCellRegex.match(formula);
    if (exactCellMatch.hasMatch()) {
        QString val = getStringValue(exactCellMatch.captured(1), exactCellMatch.captured(3).toInt()-1, colFromLetters(exactCellMatch.captured(2)));
        if (exactCellMatch.captured(1).isEmpty() && val.startsWith("=")) {
            QString evalStr = evaluateFormula(val.mid(1));
            recursionDepth--;
            return evalStr;
        }
        
        recursionDepth--;
        return val;
    }

    QString expr = formula.toUpper();
    expr.replace(" ", "");

    // 2. SUM 함수 처리: SUM(...) 을 찾아 값을 계산하고 숫자로 치환
    QRegularExpression sumRegex("SUM\\(([^)]+)\\)");
    QRegularExpressionMatch match;
    while ((match = sumRegex.match(expr)).hasMatch()) {
        QString argsStr = match.captured(1);
        QStringList args = argsStr.split(',');
        double total = 0.0;

        for (QString arg : args) {
            arg = arg.trimmed();
            QRegularExpression rangeRegex("^(?:'?([^!']+)'?!)?([A-Z]+)(\\d+):([A-Z]+)(\\d+)$");
            QRegularExpressionMatch rangeMatch = rangeRegex.match(arg);
            if (rangeMatch.hasMatch()) {
                QString sheetName = rangeMatch.captured(1);
                int col1 = colFromLetters(rangeMatch.captured(2));
                int row1 = rangeMatch.captured(3).toInt() - 1;
                int col2 = colFromLetters(rangeMatch.captured(4));
                int row2 = rangeMatch.captured(5).toInt() - 1;
                for (int r = qMin(row1, row2); r <= qMax(row1, row2); ++r) {
                    for (int c = qMin(col1, col2); c <= qMax(col1, col2); ++c) {
                        total += getNumValue(sheetName, r, c);
                    }
                }
                continue;
            }
            
            QRegularExpression cellRegex("^(?:'?([^!']+)'?!)?([A-Z]+)(\\d+)$");
            QRegularExpressionMatch cellMatch = cellRegex.match(arg);
            if (cellMatch.hasMatch()) {
                QString sheetName = cellMatch.captured(1);
                int col = colFromLetters(cellMatch.captured(2));
                int row = cellMatch.captured(3).toInt() - 1;
                total += getNumValue(sheetName, row, col);
                continue;
            }

            bool ok;
            double val = arg.toDouble(&ok);
            if (ok) total += val;
        }
        auto formatDouble = [](double v) -> QString {
            QString s = QString::number(v, 'f', 12);
            if (s.contains('.')) {
                while (s.endsWith('0')) s.chop(1);
                if (s.endsWith('.')) s.chop(1);
            }
            return s;
        };

        expr.replace(match.capturedStart(0), match.capturedLength(0), formatDouble(total));
    }

    // 3. 남은 단일 셀 참조 치환 (예: A1 + B2)
    QRegularExpression cellRefRegex("(?:(?:'([^']+)'|([^!+*/(),<>=&^% \\t']+))!)?\\b([A-Z]+)(\\d+)\\b");
    int offset = 0;
    while ((match = cellRefRegex.match(expr, offset)).hasMatch()) {
        QString sheetName = match.captured(1);
        if (sheetName.isEmpty()) sheetName = match.captured(2);
        int col = colFromLetters(match.captured(3));
        int row = match.captured(4).toInt() - 1;
        double val = getNumValue(sheetName, row, col);
        
        auto formatDouble = [](double v) -> QString {
            QString s = QString::number(v, 'f', 12);
            if (s.contains('.')) {
                while (s.endsWith('0')) s.chop(1);
                if (s.endsWith('.')) s.chop(1);
            }
            return s;
        };
        
        expr.replace(match.capturedStart(0), match.capturedLength(0), formatDouble(val));
        offset = 0; // 문자열이 변했으므로 다시 처음부터 탐색
    }

    // 4. 최종 수식 계산
    MathExpr::Parser parser;
    double result = parser.evaluate(expr);

    recursionDepth--;
    
    QString finalStr = QString::number(result, 'f', 12);
    if (finalStr.contains('.')) {
        while (finalStr.endsWith('0')) finalStr.chop(1);
        if (finalStr.endsWith('.')) finalStr.chop(1);
    }
    return finalStr;
}

QVariant SpreadsheetModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return QVariant();
    int logicalRow = m_rowMap.empty() ? index.row() : m_rowMap[index.row()];
    QPair<int, int> pos = qMakePair(logicalRow, index.column());

    if (role == Qt::DisplayRole) {
        QString val = getRawCellValue(index.row(), index.column());
        if (val.startsWith("=")) {
            val = evaluateFormula(val.mid(1));
        }

        if (m_formats.value(pos) == "Accounting") {
            bool ok;
            double d = val.toDouble(&ok);
            if (ok) {
                if (qAbs(d) < 1e-9) return "-";
                QLocale loc(QLocale::English);
                if (qFloor(d) == d) {
                    return loc.toString((qint64)d);
                } else {
                    return loc.toString(d, 'f', 2);
                }
            }
        }

        return val;
    } else if (role == Qt::UserRole + 10) {
        QString val = getRawCellValue(index.row(), index.column());
        if (val.startsWith("=")) {
            val = evaluateFormula(val.mid(1));
        }
        return val;
    } else if (role == Qt::EditRole) {
        return getRawCellValue(index.row(), index.column());
    } else if (role == Qt::FontRole) {
        if (m_fonts.contains(pos)) return m_fonts[pos];
    } else if (role == Qt::BackgroundRole) {
        if (m_bgColors.contains(pos)) return QBrush(m_bgColors[pos]);
    } else if (role == Qt::ForegroundRole) {
        if (m_fgColors.contains(pos)) return QBrush(m_fgColors[pos]);
    } else if (role == Qt::TextAlignmentRole) {
        if (m_alignments.contains(pos)) return m_alignments[pos];
        QString val = getRawCellValue(index.row(), index.column());
        bool ok;
        val.toDouble(&ok);
        return int(ok ? (Qt::AlignRight | Qt::AlignVCenter) : (Qt::AlignLeft | Qt::AlignVCenter));
    } else if (role == Qt::UserRole + 2) {
        // Check if this cell is the top-left (master) of a merged range
        for (const QRect &mr : m_mergedRanges) {
            if (mr.top() == logicalRow && mr.left() == index.column()) {
                int top = m_borders.value(pos, 0) & 0xF;
                int left = m_borders.value(pos, 0) & (0xF << 8);
                
                QPair<int, int> bottomLeft = qMakePair(mr.bottom(), mr.left());
                int bottom = m_borders.value(bottomLeft, 0) & (0xF << 4);
                
                QPair<int, int> topRight = qMakePair(mr.top(), mr.right());
                int right = m_borders.value(topRight, 0) & (0xF << 12);
                
                return top | bottom | left | right;
            }
        }
        return m_borders.value(pos, 0);
    } else if (role == Qt::UserRole + 4) {
        if (m_verticalTexts.contains(pos)) return m_verticalTexts[pos];
    }
    return QVariant();
}

QVariant SpreadsheetModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            QString header; int temp = section;
            while (temp >= 0) { header.prepend(QChar('A' + (temp % 26))); temp = (temp / 26) - 1; }
            return header;
        } else {
            return QString::number(section + 1);
        }
    }
    return QVariant();
}

QSet<QPair<int, int>> SpreadsheetModel::expandToMergedPositions(const QModelIndexList &indexes) const {
    QSet<QPair<int, int>> posSet;
    for (const QModelIndex &idx : indexes) {
        if (!idx.isValid()) continue;
        int vRow = idx.row();
        int col = idx.column();
        bool inMerge = false;
        for (const QRect &r : m_mergedRanges) {
            if (r.x() <= col && col < r.x() + r.width() &&
                r.y() <= vRow && vRow < r.y() + r.height()) {
                inMerge = true;
                for (int vr = r.y(); vr < r.y() + r.height(); ++vr) {
                    for (int vc = r.x(); vc < r.x() + r.width(); ++vc) {
                        int logicalRow = m_rowMap.empty() ? vr : (vr < (int)m_rowMap.size() ? m_rowMap[vr] : vr);
                        posSet.insert(qMakePair(logicalRow, vc));
                    }
                }
                break;
            }
        }
        if (!inMerge) {
            int logicalRow = m_rowMap.empty() ? vRow : (vRow < (int)m_rowMap.size() ? m_rowMap[vRow] : vRow);
            posSet.insert(qMakePair(logicalRow, col));
        }
    }
    return posSet;
}

void SpreadsheetModel::setCellBold(const QModelIndexList &indexes) {
    if (indexes.isEmpty()) return;
    pushUndo();
    QSet<QPair<int, int>> targetPositions = expandToMergedPositions(indexes);
    for (const auto &pos : targetPositions) {
        QFont f = m_fonts.value(pos, QFont()); f.setBold(!f.bold()); m_fonts[pos] = f;
    }
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::FontRole});
}
void SpreadsheetModel::setCellItalic(const QModelIndexList &indexes) {
    if (indexes.isEmpty()) return;
    pushUndo();
    QSet<QPair<int, int>> targetPositions = expandToMergedPositions(indexes);
    for (const auto &pos : targetPositions) {
        QFont f = m_fonts.value(pos, QFont()); f.setItalic(!f.italic()); m_fonts[pos] = f;
    }
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::FontRole});
}
void SpreadsheetModel::setFontSize(const QModelIndexList &indexes, int size) {
    if (indexes.isEmpty()) return;
    pushUndo();
    QSet<QPair<int, int>> targetPositions = expandToMergedPositions(indexes);
    for (const auto &pos : targetPositions) {
        QFont f = m_fonts.value(pos, QFont()); f.setPointSize(size); m_fonts[pos] = f;
    }
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::FontRole});
}
void SpreadsheetModel::setCellBackgroundColor(const QModelIndexList &indexes, const QColor &color) {
    if (indexes.isEmpty()) return;
    pushUndo();
    QSet<QPair<int, int>> targetPositions = expandToMergedPositions(indexes);
    for (const auto &pos : targetPositions) {
        if (color.isValid()) m_bgColors[pos] = color;
        else m_bgColors.remove(pos);
    }
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::BackgroundRole});
}
void SpreadsheetModel::setCellTextColor(const QModelIndexList &indexes, const QColor &color) {
    if (indexes.isEmpty()) return;
    pushUndo();
    QSet<QPair<int, int>> targetPositions = expandToMergedPositions(indexes);
    for (const auto &pos : targetPositions) {
        if (color.isValid()) m_fgColors[pos] = color;
        else m_fgColors.remove(pos);
    }
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::ForegroundRole});
}
void SpreadsheetModel::setCellAlignment(const QModelIndexList &indexes, int alignment) {
    if (indexes.isEmpty()) return;
    pushUndo();
    QSet<QPair<int, int>> targetPositions = expandToMergedPositions(indexes);
    for (const auto &pos : targetPositions) {
        m_alignments[pos] = alignment;
    }
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::TextAlignmentRole});
}
void SpreadsheetModel::applyBorders(const QModelIndexList &indexes, const QString &type) {
    if (indexes.isEmpty()) return;
    pushUndo();
    
    int minR = INT_MAX, maxR = -1;
    int minC = INT_MAX, maxC = -1;
    for (const QModelIndex &idx : indexes) {
        if (idx.row() < minR) minR = idx.row();
        if (idx.row() > maxR) maxR = idx.row();
        if (idx.column() < minC) minC = idx.column();
        if (idx.column() > maxC) maxC = idx.column();
    }

    int style = BorderThin;
    if (type.startsWith("Thick")) style = BorderThick;
    else if (type.startsWith("Double")) style = BorderDouble;
    
    QString action = type;
    action.replace("Thin", "");
    action.replace("Thick", "");
    action.replace("Double", "");

    auto setEdge = [&](int r, int c, int edge, int st) {
        if (r < 0 || c < 0) return;
        int logicalRow = m_rowMap.empty() ? r : (r < (int)m_rowMap.size() ? m_rowMap[r] : r);
        QPair<int, int> pos = qMakePair(logicalRow, c);
        int val = m_borders.value(pos, 0);
        int shift = (edge == 0) ? 0 : (edge == 1) ? 4 : (edge == 2) ? 8 : 12; // 0=Top, 1=Bottom, 2=Left, 3=Right
        int mask = ~(0xF << shift);
        val = (val & mask) | (st << shift);
        if (val == 0) m_borders.remove(pos);
        else m_borders[pos] = val;
    };

    auto applyTop = [&](int r, int c, int st) {
        if (r == 0) setEdge(0, c, 0, st); // Top edge of sheet
        else {
            setEdge(r, c, 0, 0); // Clear own top
            setEdge(r - 1, c, 1, st); // Set neighbor's bottom
        }
    };
    auto applyBottom = [&](int r, int c, int st) {
        if (r + 1 < rowCount()) setEdge(r + 1, c, 0, 0); // Clear neighbor's top
        setEdge(r, c, 1, st);
    };
    auto applyLeft = [&](int r, int c, int st) {
        if (c == 0) setEdge(r, 0, 2, st); // Left edge of sheet
        else {
            setEdge(r, c, 2, 0); // Clear own left
            setEdge(r, c - 1, 3, st); // Set neighbor's right
        }
    };
    auto applyRight = [&](int r, int c, int st) {
        if (c + 1 < columnCount()) setEdge(r, c + 1, 2, 0); // Clear neighbor's left
        setEdge(r, c, 3, st);
    };

    for (const QModelIndex &idx : indexes) {
        if (!idx.isValid()) continue;
        int r = idx.row();
        int c = idx.column();

        if (action == "None") {
            applyTop(r, c, 0); applyBottom(r, c, 0); applyLeft(r, c, 0); applyRight(r, c, 0);
        } else if (action == "All") {
            applyTop(r, c, style); applyBottom(r, c, style); applyLeft(r, c, style); applyRight(r, c, style);
        } else if (action == "Outside") {
            if (r == minR) applyTop(r, c, style);
            if (r == maxR) applyBottom(r, c, style);
            if (c == minC) applyLeft(r, c, style);
            if (c == maxC) applyRight(r, c, style);
        } else if (action == "Inside") {
            if (r != minR) applyTop(r, c, style);
            if (r != maxR) applyBottom(r, c, style);
            if (c != minC) applyLeft(r, c, style);
            if (c != maxC) applyRight(r, c, style);
        } else if (action == "Top") {
            if (r == minR) applyTop(r, c, style);
        } else if (action == "Bottom") {
            if (r == maxR) applyBottom(r, c, style);
        } else if (action == "Left") {
            if (c == minC) applyLeft(r, c, style);
        } else if (action == "Right") {
            if (c == maxC) applyRight(r, c, style);
        }
    }
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::UserRole + 2});
}
bool SpreadsheetModel::isCoveredByMerge(int logicalRow, int col, QPair<int, int> *topLeft) const {
    for (const QRect &r : m_mergedRanges) {
        if (r.x() <= col && col < r.x() + r.width() &&
            r.y() <= logicalRow && logicalRow < r.y() + r.height()) {
            if (topLeft) {
                *topLeft = qMakePair(r.y(), r.x());
            }
            if (logicalRow == r.y() && col == r.x()) {
                return false;
            }
            return true;
        }
    }
    return false;
}
void SpreadsheetModel::addMergedRange(const QRect &range) {
    if (!range.isValid() || range.isEmpty()) return;
    pushUndo();
    m_mergedRanges.append(range);
    
    int topLogicalRow = m_rowMap.empty() ? range.y() : (range.y() < (int)m_rowMap.size() ? m_rowMap[range.y()] : range.y());
    QPair<int, int> topLeftPos = qMakePair(topLogicalRow, range.x());
    bool hasFont = m_fonts.contains(topLeftPos);
    QFont f = hasFont ? m_fonts[topLeftPos] : QFont();
    bool hasBg = m_bgColors.contains(topLeftPos);
    QColor bg = hasBg ? m_bgColors[topLeftPos] : QColor();
    bool hasFg = m_fgColors.contains(topLeftPos);
    QColor fg = hasFg ? m_fgColors[topLeftPos] : QColor();
    bool hasAlign = m_alignments.contains(topLeftPos);
    int align = hasAlign ? m_alignments[topLeftPos] : 0;

    for (int r = range.y(); r < range.y() + range.height(); ++r) {
        int logicalRow = m_rowMap.empty() ? r : (r < (int)m_rowMap.size() ? m_rowMap[r] : r);
        for (int c = range.x(); c < range.x() + range.width(); ++c) {
            if (r == range.y() && c == range.x()) continue;
            QPair<int, int> pos = qMakePair(logicalRow, c);
            m_editedData.remove(pos);
            if (hasFont) m_fonts[pos] = f; else m_fonts.remove(pos);
            if (hasBg) m_bgColors[pos] = bg; else m_bgColors.remove(pos);
            if (hasFg) m_fgColors[pos] = fg; else m_fgColors.remove(pos);
            if (hasAlign) m_alignments[pos] = align; else m_alignments.remove(pos);
        }
    }
}
void SpreadsheetModel::removeMergedRange(const QRect &range) {
    pushUndo();
    m_mergedRanges.removeAll(range);
}

bool SpreadsheetModel::insertRows(int row, int count, const QModelIndex &parent) {
    if (row < 0 || count <= 0) return false;
    pushUndo();
    beginInsertRows(parent, row, row + count - 1);
    updateMergedRangesOnInsertRows(row, count);
    
    int maxLogical = -1;
    for (int l : m_rowMap) {
        if (l > maxLogical) maxLogical = l;
    }
    
    for (int i = 0; i < count; ++i) {
        int newLogical = maxLogical + 1 + i;
        if (row + i <= (int)m_rowMap.size()) {
            m_rowMap.insert(m_rowMap.begin() + row + i, newLogical);
            if (m_rowMap.size() - 1 == m_rangeRowMap.size()) {
                m_rangeRowMap.insert(m_rangeRowMap.begin() + row + i, newLogical);
            }
        } else {
            m_rowMap.push_back(newLogical);
            m_rangeRowMap.push_back(newLogical);
        }
    }
    endInsertRows();
    return true;
}

bool SpreadsheetModel::removeRows(int row, int count, const QModelIndex &parent) {
    if (row < 0 || count <= 0 || row + count > (int)m_rowMap.size()) return false;
    pushUndo();
    beginRemoveRows(parent, row, row + count - 1);
    updateMergedRangesOnRemoveRows(row, count);
    
    m_rowMap.erase(m_rowMap.begin() + row, m_rowMap.begin() + row + count);
    if (row + count <= (int)m_rangeRowMap.size()) {
        m_rangeRowMap.erase(m_rangeRowMap.begin() + row, m_rangeRowMap.begin() + row + count);
    }
    endRemoveRows();
    return true;
}

template <typename T>
void shiftHashColumns(QHash<QPair<int, int>, T> &hash, int col, int count, bool isInsert) {
    QList<QPair<int, int>> keys = hash.keys();
    std::sort(keys.begin(), keys.end(), [isInsert](const QPair<int, int> &a, const QPair<int, int> &b) {
        return isInsert ? (a.second > b.second) : (a.second < b.second);
    });
    
    for (const auto &key : keys) {
        if (isInsert) {
            if (key.second >= col) {
                T val = hash.take(key);
                hash[qMakePair(key.first, key.second + count)] = val;
            }
        } else {
            if (key.second >= col && key.second < col + count) {
                hash.remove(key);
            } else if (key.second >= col + count) {
                T val = hash.take(key);
                hash[qMakePair(key.first, key.second - count)] = val;
            }
        }
    }
}

bool SpreadsheetModel::insertColumns(int column, int count, const QModelIndex &parent) {
    if (column < 0 || count <= 0) return false;
    pushUndo();
    beginInsertColumns(parent, column, column + count - 1);
    updateMergedRangesOnInsertCols(column, count);
    shiftHashColumns(m_editedData, column, count, true);
    shiftHashColumns(m_fonts, column, count, true);
    shiftHashColumns(m_bgColors, column, count, true);
    shiftHashColumns(m_fgColors, column, count, true);
    shiftHashColumns(m_alignments, column, count, true);
    m_columnCount += count;
    endInsertColumns();
    return true;
}

bool SpreadsheetModel::removeColumns(int column, int count, const QModelIndex &parent) {
    if (column < 0 || count <= 0 || column + count > m_columnCount) return false;
    pushUndo();
    beginRemoveColumns(parent, column, column + count - 1);
    updateMergedRangesOnRemoveCols(column, count);
    shiftHashColumns(m_editedData, column, count, false);
    shiftHashColumns(m_fonts, column, count, false);
    shiftHashColumns(m_bgColors, column, count, false);
    shiftHashColumns(m_fgColors, column, count, false);
    shiftHashColumns(m_alignments, column, count, false);
    m_columnCount -= count;
    endRemoveColumns();
    return true;
}

void SpreadsheetModel::updateMergedRangesOnInsertRows(int row, int count) {
    for (int i = 0; i < m_mergedRanges.size(); ++i) {
        QRect &r = m_mergedRanges[i];
        if (r.y() >= row) {
            r.moveTop(r.y() + count);
        } else if (row > r.y() && row < r.y() + r.height()) {
            r.setHeight(r.height() + count);
        }
    }
}

void SpreadsheetModel::updateMergedRangesOnRemoveRows(int row, int count) {
    for (int i = m_mergedRanges.size() - 1; i >= 0; --i) {
        QRect &r = m_mergedRanges[i];
        if (r.y() >= row + count) {
            r.moveTop(r.y() - count);
        } else if (r.y() >= row && r.y() + r.height() <= row + count) {
            m_mergedRanges.removeAt(i);
        } else if (row <= r.y() && row + count > r.y()) {
            int newY = row;
            int newH = r.y() + r.height() - (row + count);
            r.setRect(r.x(), newY, r.width(), qMax(1, newH));
        } else if (row > r.y() && row < r.y() + r.height()) {
            int removedInside = qMin(count, r.y() + r.height() - row);
            r.setHeight(qMax(1, r.height() - removedInside));
        }
        if (r.width() <= 1 && r.height() <= 1) {
            m_mergedRanges.removeAt(i);
        }
    }
}

void SpreadsheetModel::updateMergedRangesOnInsertCols(int col, int count) {
    for (int i = 0; i < m_mergedRanges.size(); ++i) {
        QRect &r = m_mergedRanges[i];
        if (r.x() >= col) {
            r.moveLeft(r.x() + count);
        } else if (col > r.x() && col < r.x() + r.width()) {
            r.setWidth(r.width() + count);
        }
    }
}

void SpreadsheetModel::updateMergedRangesOnRemoveCols(int col, int count) {
    for (int i = m_mergedRanges.size() - 1; i >= 0; --i) {
        QRect &r = m_mergedRanges[i];
        if (r.x() >= col + count) {
            r.moveLeft(r.x() - count);
        } else if (r.x() >= col && r.x() + r.width() <= col + count) {
            m_mergedRanges.removeAt(i);
        } else if (col <= r.x() && col + count > r.x()) {
            int newX = col;
            int newW = r.x() + r.width() - (col + count);
            r.setRect(newX, r.y(), qMax(1, newW), r.height());
        } else if (col > r.x() && col < r.x() + r.width()) {
            int removedInside = qMin(count, r.x() + r.width() - col);
            r.setWidth(qMax(1, r.width() - removedInside));
        }
        if (r.width() <= 1 && r.height() <= 1) {
            m_mergedRanges.removeAt(i);
        }
    }
}

void SpreadsheetModel::setFilterRange(int startRow, int startCol, int endCol) {
    pushUndo();
    emit layoutAboutToBeChanged();
    m_filterHeaderRow = startRow;
    m_filterStartCol = startCol;
    m_filterEndCol = endCol;
    m_rangeRowMap = m_rowMap; // 필터 지정 시 현재 상태로 리셋
    emit layoutChanged();
}

void SpreadsheetModel::clearFilter() {
    if (m_filterHeaderRow == -1) return;
    pushUndo();
    emit layoutAboutToBeChanged();
    m_filterHeaderRow = -1;
    m_filterStartCol = -1;
    m_filterEndCol = -1;
    
    // 엑셀 표준: 필터를 꺼도, 부분 정렬했던 결과는 원래 시트에 그대로 보존됩니다.
    if (m_rowMap.size() == m_rangeRowMap.size()) {
        for (size_t i = 0; i < m_rowMap.size(); ++i) {
            m_rowMap[i] = m_rangeRowMap[i];
        }
    }
    emit layoutChanged();
}

bool SpreadsheetModel::isFilterHeader(int row, int col) const {
    return row == m_filterHeaderRow && col >= m_filterStartCol && col <= m_filterEndCol;
}

void SpreadsheetModel::sortRange(int column, Qt::SortOrder order) {
    if (m_filterHeaderRow == -1 || m_rangeRowMap.empty()) return;
    int dataStartRow = m_filterHeaderRow + 1; // 필터 헤더행 하위 데이터들만 정렬
    if (dataStartRow >= (int)m_rangeRowMap.size()) return;
    pushUndo();

    beginResetModel(); // 강제 UI 리로드를 통해 정렬 결과를 확실히 반영
    
    auto beginIt = m_rangeRowMap.begin() + dataStartRow;
    auto endIt = m_rangeRowMap.end();

    // 값을 캐싱하여 디스크 접근 성능 문제 해결
    std::vector<QPair<QString, int>> cachedValues;
    cachedValues.reserve(std::distance(beginIt, endIt));
    for (auto it = beginIt; it != endIt; ++it) {
        int lRow = *it;
        QString v = getCellData(lRow, column);
        if (m_editedData.contains(qMakePair(lRow, column))) v = m_editedData[qMakePair(lRow, column)];
        cachedValues.push_back(qMakePair(v, lRow));
    }
    
    std::sort(cachedValues.begin(), cachedValues.end(), [order](const QPair<QString, int> &a, const QPair<QString, int> &b) {
        bool aEmpty = a.first.trimmed().isEmpty();
        bool bEmpty = b.first.trimmed().isEmpty();
        
        // 엑셀 표준: 빈 셀은 오름차순/내림차순 상관없이 무조건 맨 아래로
        if (aEmpty && !bEmpty) return false;
        if (!aEmpty && bEmpty) return true;
        if (aEmpty && bEmpty) return a.second < b.second; // 둘 다 비었으면 원래 순서 유지

        bool ok1, ok2;
        double d1 = a.first.toDouble(&ok1);
        double d2 = b.first.toDouble(&ok2);
        if (ok1 && ok2) {
            return order == Qt::AscendingOrder ? (d1 < d2) : (d1 > d2);
        }
        return order == Qt::AscendingOrder ? (a.first.compare(b.first, Qt::CaseInsensitive) < 0) : (a.first.compare(b.first, Qt::CaseInsensitive) > 0);
    });
    
    // 캐싱된 결과에 맞춰 맵핑 인덱스 업데이트
    for (size_t i = 0; i < cachedValues.size(); ++i) {
        *(beginIt + i) = cachedValues[i].second;
        if (dataStartRow + i < m_rowMap.size()) {
            m_rowMap[dataStartRow + i] = cachedValues[i].second;
        }
    }

    endResetModel();
}

bool SpreadsheetModel::saveCsv(const QString &filePath) {
    QString tmpPath = filePath + ".tmp";
    QFile outFile(tmpPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    
    QTextStream out(&outFile);
    out.setEncoding(QStringConverter::Utf8);
    
    int maxR = rowCount() - 1;
    int maxC = 10;
    if (m_file && m_file->isOpen()) {
        maxC = m_columnCount - 1;
    }
    for (auto it = m_editedData.begin(); it != m_editedData.end(); ++it) {
        if (it.key().second > maxC) maxC = it.key().second;
        if (it.key().first > maxR) maxR = it.key().first;
    }

    for (int r = 0; r <= maxR; ++r) {
        QStringList rowVals;
        bool hasData = false;
        for (int c = 0; c <= maxC; ++c) {
            QString val = getRawCellValue(r, c);
            if (!val.isEmpty()) hasData = true;
            if (val.contains(',') || val.contains('"') || val.contains('\n')) {
                val.replace('"', "\"\"");
                val = "\"" + val + "\"";
            }
            rowVals.append(val);
        }
        if (!hasData && r >= (int)m_lineOffsets.size() && r > 100) break;
        out << rowVals.join(',') << "\n";
    }
    outFile.close();

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    
    if (QFile::exists(filePath)) {
        QFile::remove(filePath);
    }
    bool res = QFile::rename(tmpPath, filePath);
    if (res) {
        loadCsv(filePath);
    }
    return res;
}

bool SpreadsheetModel::saveCx(const QString &filePath) {
    QString tmpPath = filePath + ".tmp";
    QFile outFile(tmpPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QJsonObject root = toJsonObject();

    QJsonDocument doc(root);
    outFile.write(doc.toJson(QJsonDocument::Compact));
    outFile.close();

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    if (QFile::exists(filePath)) {
        QFile::remove(filePath);
    }
    bool res = QFile::rename(tmpPath, filePath);
    if (res) {
        loadCx(filePath);
    }
    return res;
}

bool SpreadsheetModel::loadCx(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    
    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    bool res = fromJsonObject(doc.object());
    clearUndoStack();
    return res;
}

QJsonObject SpreadsheetModel::toJsonObject() const {
    QJsonObject root;
    root["version"] = "1.0";
    root["rows"] = rowCount();
    root["cols"] = columnCount();

    if (m_filterHeaderRow != -1) {
        root["filterRow"] = m_filterHeaderRow;
        root["filterStartCol"] = m_filterStartCol;
        root["filterEndCol"] = m_filterEndCol;
    }

    int maxR = rowCount() - 1;
    int maxC = 25;
    for (auto it = m_editedData.begin(); it != m_editedData.end(); ++it) {
        if (it.key().second > maxC) maxC = it.key().second;
        if (it.key().first > maxR) maxR = it.key().first;
    }
    for (auto it = m_fonts.begin(); it != m_fonts.end(); ++it) {
        if (it.key().second > maxC) maxC = it.key().second;
        if (it.key().first > maxR) maxR = it.key().first;
    }
    for (auto it = m_bgColors.begin(); it != m_bgColors.end(); ++it) {
        if (it.key().second > maxC) maxC = it.key().second;
        if (it.key().first > maxR) maxR = it.key().first;
    }
    for (auto it = m_fgColors.begin(); it != m_fgColors.end(); ++it) {
        if (it.key().second > maxC) maxC = it.key().second;
        if (it.key().first > maxR) maxR = it.key().first;
    }
    for (auto it = m_alignments.begin(); it != m_alignments.end(); ++it) {
        if (it.key().second > maxC) maxC = it.key().second;
        if (it.key().first > maxR) maxR = it.key().first;
    }
    for (auto it = m_borders.begin(); it != m_borders.end(); ++it) {
        if (it.key().second > maxC) maxC = it.key().second;
        if (it.key().first > maxR) maxR = it.key().first;
    }
    if (m_file && m_file->isOpen()) {
        maxC = qMax(maxC, m_columnCount - 1);
    }

    QJsonArray cellsArray;
    QJsonArray fgs;
    QJsonArray aligns;
    QJsonArray borders;
    QJsonArray vTextsArr;
    QJsonArray formatsArr;
    for (int r = 0; r <= maxR; ++r) {
        for (int c = 0; c <= maxC; ++c) {
            QString val = getRawCellValue(r, c);
            QPair<int, int> pos = qMakePair(m_rowMap.empty() ? r : m_rowMap[r], c);
            bool hasFont = m_fonts.contains(pos);
            bool hasBg = m_bgColors.contains(pos);
            bool hasFg = m_fgColors.contains(pos);
            bool hasAlign = m_alignments.contains(pos);
            bool hasBorder = m_borders.contains(pos);
            if (val.isEmpty() && !hasFont && !hasBg && !hasFg && !hasAlign && !hasBorder) continue;

            QJsonObject cellObj;
            cellObj["r"] = pos.first;
            cellObj["c"] = c;
            if (!val.isEmpty()) cellObj["v"] = val;
            if (hasFont) {
                QFont f = m_fonts[pos];
                QJsonObject fontObj;
                fontObj["size"] = f.pointSize();
                fontObj["bold"] = f.bold();
                fontObj["italic"] = f.italic();
                cellObj["font"] = fontObj;
            }
            if (hasBg) cellObj["bg"] = m_bgColors[pos].name(QColor::HexArgb);
            
            if (hasFg) {
                QJsonObject fg; fg["r"] = pos.first; fg["c"] = c; fg["val"] = m_fgColors[pos].name(QColor::HexArgb);
                fgs.append(fg);
            }
            if (hasAlign) {
                QJsonObject al; al["r"] = pos.first; al["c"] = c; al["val"] = m_alignments[pos];
                aligns.append(al);
            }
            if (hasBorder) {
                QJsonObject bd; bd["r"] = pos.first; bd["c"] = c; bd["val"] = m_borders[pos];
                borders.append(bd);
            }
            if (m_verticalTexts.contains(pos)) {
                QJsonObject vt; vt["r"] = pos.first; vt["c"] = c; vt["val"] = m_verticalTexts[pos];
                vTextsArr.append(vt);
            }
            if (m_formats.contains(pos)) {
                QJsonObject fmt; fmt["r"] = pos.first; fmt["c"] = c; fmt["val"] = m_formats[pos];
                formatsArr.append(fmt);
            }
            cellsArray.append(cellObj);
        }
    }
    root["cells"] = cellsArray;
    root["fgs"] = fgs;
    root["aligns"] = aligns;
    root["borders"] = borders;
    root["verticalTexts"] = vTextsArr;
    root["formats"] = formatsArr;

    QJsonArray mergeArray;
    for (const QRect &r : m_mergedRanges) {
        QJsonObject mObj;
        mObj["x"] = r.x(); mObj["y"] = r.y(); mObj["w"] = r.width(); mObj["h"] = r.height();
        mergeArray.append(mObj);
    }
    root["merged"] = mergeArray;

    QJsonArray rowMapArray;
    for (int l : m_rowMap) rowMapArray.append(l);
    root["rowMap"] = rowMapArray;
    QJsonArray rangeRowMapArray;
    for (int l : m_rangeRowMap) rangeRowMapArray.append(l);
    root["rangeRowMap"] = rangeRowMapArray;
    
    QJsonObject rowHeightsObj;
    for (auto it = m_rowHeights.begin(); it != m_rowHeights.end(); ++it) {
        rowHeightsObj[QString::number(it.key())] = it.value();
    }
    root["rowHeights"] = rowHeightsObj;
    
    QJsonObject colWidthsObj;
    for (auto it = m_colWidths.begin(); it != m_colWidths.end(); ++it) {
        colWidthsObj[QString::number(it.key())] = it.value();
    }
    root["colWidths"] = colWidthsObj;
    
    QJsonObject cursorObj;
    cursorObj["r"] = m_cursorPos.first;
    cursorObj["c"] = m_cursorPos.second;
    root["cursor"] = cursorObj;
    
    return root;
}

bool SpreadsheetModel::fromJsonObject(const QJsonObject &root) {
    beginResetModel();
    m_lineOffsets.clear();
    m_editedData.clear();
    m_fonts.clear();
    m_bgColors.clear();
    m_fgColors.clear();
    m_alignments.clear();
    m_borders.clear();
    m_mergedRanges.clear();
    m_filterHeaderRow = root.contains("filterRow") ? root["filterRow"].toInt(-1) : -1;
    m_filterStartCol = root.contains("filterStartCol") ? root["filterStartCol"].toInt(-1) : -1;
    m_filterEndCol = root.contains("filterEndCol") ? root["filterEndCol"].toInt(-1) : -1;

    int rows = root["rows"].toInt(100);
    int cols = root["cols"].toInt(16384);
    m_columnCount = cols > 0 ? cols : 16384;

    m_rowMap.clear();
    if (root.contains("rowMap")) {
        QJsonArray rowMapArray = root["rowMap"].toArray();
        for (const QJsonValue &val : rowMapArray) {
            m_rowMap.push_back(val.toInt());
        }
    } else {
        for (int i = 0; i < rows; ++i) {
            m_rowMap.push_back(i);
        }
    }
    m_rangeRowMap.clear();
    if (root.contains("rangeRowMap")) {
        QJsonArray rangeRowMapArray = root["rangeRowMap"].toArray();
        for (const QJsonValue &val : rangeRowMapArray) {
            m_rangeRowMap.push_back(val.toInt());
        }
    } else {
        m_rangeRowMap = m_rowMap;
    }

    QJsonArray cellsArray = root["cells"].toArray();
    for (const QJsonValue &val : cellsArray) {
        QJsonObject cellObj = val.toObject();
        int r = cellObj["r"].toInt();
        int c = cellObj["c"].toInt();
        QString v = cellObj["v"].toString();
        
        QPair<int, int> pos = qMakePair(r, c);
        if (!v.isEmpty()) {
            m_editedData[pos] = v;
        }
        if (cellObj.contains("font")) {
            QJsonObject fontObj = cellObj["font"].toObject();
            QFont f;
            if (fontObj.contains("size")) f.setPointSize(fontObj["size"].toInt(10));
            if (fontObj.contains("bold")) f.setBold(fontObj["bold"].toBool());
            if (fontObj.contains("italic")) f.setItalic(fontObj["italic"].toBool());
            m_fonts[pos] = f;
        }
        if (cellObj.contains("bg")) m_bgColors[pos] = QColor(cellObj["bg"].toString());
    }

    QJsonArray fgsArr = root["fgs"].toArray();
    for (int i = 0; i < fgsArr.size(); ++i) {
        QJsonObject obj = fgsArr[i].toObject();
        m_fgColors[qMakePair(obj["r"].toInt(), obj["c"].toInt())] = QColor(obj["val"].toString());
    }
    QJsonArray alignsArr = root["aligns"].toArray();
    for (int i = 0; i < alignsArr.size(); ++i) {
        QJsonObject obj = alignsArr[i].toObject();
        m_alignments[qMakePair(obj["r"].toInt(), obj["c"].toInt())] = obj["val"].toInt();
    }
    QJsonArray bordersArr = root["borders"].toArray();
    for (int i = 0; i < bordersArr.size(); ++i) {
        QJsonObject obj = bordersArr[i].toObject();
        m_borders[qMakePair(obj["r"].toInt(), obj["c"].toInt())] = obj["val"].toInt();
    }
    m_verticalTexts.clear();
    QJsonArray vTextsArr = root["verticalTexts"].toArray();
    for (int i = 0; i < vTextsArr.size(); ++i) {
        QJsonObject obj = vTextsArr[i].toObject();
        m_verticalTexts[qMakePair(obj["r"].toInt(), obj["c"].toInt())] = obj["val"].toBool();
    }
    
    m_formats.clear();
    QJsonArray formatsArr = root["formats"].toArray();
    for (int i = 0; i < formatsArr.size(); ++i) {
        QJsonObject obj = formatsArr[i].toObject();
        m_formats[qMakePair(obj["r"].toInt(), obj["c"].toInt())] = obj["val"].toString();
    }
    
    m_rowHeights.clear();
    if (root.contains("rowHeights")) {
        QJsonObject rowHeightsObj = root["rowHeights"].toObject();
        for (auto it = rowHeightsObj.begin(); it != rowHeightsObj.end(); ++it) {
            m_rowHeights[it.key().toInt()] = it.value().toInt();
        }
    }
    
    m_colWidths.clear();
    if (root.contains("colWidths")) {
        QJsonObject colWidthsObj = root["colWidths"].toObject();
        for (auto it = colWidthsObj.begin(); it != colWidthsObj.end(); ++it) {
            m_colWidths[it.key().toInt()] = it.value().toInt();
        }
    }

    QJsonArray mergeArray = root["merged"].toArray();
    for (const QJsonValue &val : mergeArray) {
        QJsonObject mObj = val.toObject();
        m_mergedRanges.append(QRect(mObj["x"].toInt(), mObj["y"].toInt(), mObj["w"].toInt(), mObj["h"].toInt()));
    }

    if (root.contains("cursor")) {
        QJsonObject cursorObj = root["cursor"].toObject();
        m_cursorPos = qMakePair(cursorObj["r"].toInt(0), cursorObj["c"].toInt(0));
    } else {
        m_cursorPos = qMakePair(0, 0);
    }

    endResetModel();
    emit sizesRestored();
    return true;
}

void SpreadsheetModel::copyFrom(const SpreadsheetModel *src) {
    if (!src) return;
    fromJsonObject(src->toJsonObject());
    clearUndoStack();
}

void SpreadsheetModel::collectStyles(QXmlStreamWriter &xml, QHash<QString, QString> &styleToId, int &styleIdx) const {
    int maxR = rowCount() - 1;
    int maxC = 10;
    if (m_file && m_file->isOpen()) maxC = m_columnCount - 1;
    for (auto it = m_editedData.begin(); it != m_editedData.end(); ++it) {
        if (it.key().second > maxC) maxC = it.key().second;
        if (it.key().first > maxR) maxR = it.key().first;
    }
    for (auto it = m_bgColors.begin(); it != m_bgColors.end(); ++it) {
        if (it.key().second > maxC) maxC = it.key().second;
        if (it.key().first > maxR) maxR = it.key().first;
    }
    for (auto it = m_fgColors.begin(); it != m_fgColors.end(); ++it) {
        if (it.key().second > maxC) maxC = it.key().second;
        if (it.key().first > maxR) maxR = it.key().first;
    }

    for (int r = 0; r <= maxR; ++r) {
        for (int c = 0; c <= maxC; ++c) {
            QPair<int, int> pos = qMakePair(m_rowMap.empty() ? r : m_rowMap[r], c);
            bool hasF = m_fonts.contains(pos);
            bool hasBg = m_bgColors.contains(pos);
            bool hasFg = m_fgColors.contains(pos);
            bool hasAl = m_alignments.contains(pos);
            if (!hasF && !hasBg && !hasFg && !hasAl) continue;

            QFont f = m_fonts.value(pos, QFont());
            QColor bg = m_bgColors.value(pos, QColor());
            QColor fg = m_fgColors.value(pos, QColor());
            int al = m_alignments.value(pos, 0);
            QString key = QString("%1_%2_%3_%4_%5_%6").arg(f.pointSize()).arg(f.bold()).arg(f.italic()).arg(bg.name(QColor::HexRgb)).arg(fg.name(QColor::HexRgb)).arg(al);
            if (!styleToId.contains(key)) {
                QString styleId = QString("s%1").arg(styleIdx++);
                styleToId[key] = styleId;

                xml.writeStartElement("Style");
                xml.writeAttribute("ss:ID", styleId);
                if (hasAl) {
                    xml.writeEmptyElement("Alignment");
                    if (al & Qt::AlignRight) xml.writeAttribute("ss:Horizontal", "Right");
                    else if (al & Qt::AlignHCenter) xml.writeAttribute("ss:Horizontal", "Center");
                    else xml.writeAttribute("ss:Horizontal", "Left");
                    xml.writeAttribute("ss:Vertical", "Center");
                }
                xml.writeEmptyElement("Font");
                xml.writeAttribute("ss:FontName", "Malgun Gothic");
                xml.writeAttribute("ss:Size", QString::number(f.pointSize() > 0 ? f.pointSize() : 10));
                if (f.bold()) xml.writeAttribute("ss:Bold", "1");
                if (f.italic()) xml.writeAttribute("ss:Italic", "1");
                if (hasFg) xml.writeAttribute("ss:Color", fg.name(QColor::HexRgb));
                if (hasBg) {
                    xml.writeEmptyElement("Interior");
                    xml.writeAttribute("ss:Color", bg.name(QColor::HexRgb));
                    xml.writeAttribute("ss:Pattern", "Solid");
                }
                xml.writeEndElement(); // Style
            }
        }
    }
}

void SpreadsheetModel::exportWorksheetToXml(QXmlStreamWriter &xml, const QString &sheetName, QHash<QString, QString> &styleToId, int &styleIdx) {
    int maxR = rowCount() - 1;
    int maxC = 10;
    if (m_file && m_file->isOpen()) maxC = m_columnCount - 1;
    for (auto it = m_editedData.begin(); it != m_editedData.end(); ++it) {
        if (it.key().second > maxC) maxC = it.key().second;
        if (it.key().first > maxR) maxR = it.key().first;
    }
    for (auto it = m_bgColors.begin(); it != m_bgColors.end(); ++it) {
        if (it.key().second > maxC) maxC = it.key().second;
        if (it.key().first > maxR) maxR = it.key().first;
    }
    for (auto it = m_fgColors.begin(); it != m_fgColors.end(); ++it) {
        if (it.key().second > maxC) maxC = it.key().second;
        if (it.key().first > maxR) maxR = it.key().first;
    }

    xml.writeStartElement("Worksheet");
    xml.writeAttribute("ss:Name", sheetName);
    xml.writeStartElement("Table");

    for (int r = 0; r <= maxR; ++r) {
        bool rowHasData = false;
        for (int c = 0; c <= maxC; ++c) {
            QString val = getRawCellValue(r, c);
            QPair<int, int> pos = qMakePair(m_rowMap.empty() ? r : m_rowMap[r], c);
            if (!val.isEmpty() || m_fonts.contains(pos) || m_bgColors.contains(pos) || m_fgColors.contains(pos)) {
                rowHasData = true;
                break;
            }
        }
        if (!rowHasData && r >= (int)m_lineOffsets.size() && r > 100) break;

        xml.writeStartElement("Row");
        for (int c = 0; c <= maxC; ++c) {
            // Check if cell is covered by a merge (not top-left)
            bool isCovered = false;
            QRect topMerge;
            for (const QRect &mg : m_mergedRanges) {
                if (mg.contains(c, r)) {
                    if (mg.x() != c || mg.y() != r) {
                        isCovered = true;
                    } else {
                        topMerge = mg;
                    }
                    break;
                }
            }
            if (isCovered) continue;

            QString val = getRawCellValue(r, c);
            QPair<int, int> pos = qMakePair(m_rowMap.empty() ? r : m_rowMap[r], c);
            bool hasStyle = (m_fonts.contains(pos) || m_bgColors.contains(pos) || m_fgColors.contains(pos) || m_alignments.contains(pos));
            if (val.isEmpty() && !hasStyle && !topMerge.isValid()) continue;

            xml.writeStartElement("Cell");
            xml.writeAttribute("ss:Index", QString::number(c + 1));
            if (topMerge.isValid()) {
                if (topMerge.width() > 1) xml.writeAttribute("ss:MergeAcross", QString::number(topMerge.width() - 1));
                if (topMerge.height() > 1) xml.writeAttribute("ss:MergeDown", QString::number(topMerge.height() - 1));
            }

            if (hasStyle) {
                QFont f = m_fonts.value(pos, QFont());
                QColor bg = m_bgColors.value(pos, QColor());
                QColor fg = m_fgColors.value(pos, QColor());
                int al = m_alignments.value(pos, 0);
                QString key = QString("%1_%2_%3_%4_%5_%6").arg(f.pointSize()).arg(f.bold()).arg(f.italic()).arg(bg.name(QColor::HexRgb)).arg(fg.name(QColor::HexRgb)).arg(al);
                if (styleToId.contains(key)) {
                    xml.writeAttribute("ss:StyleID", styleToId[key]);
                }
            }
            if (!val.isEmpty()) {
                xml.writeStartElement("Data");
                bool ok;
                val.toDouble(&ok);
                xml.writeAttribute("ss:Type", ok ? "Number" : "String");
                xml.writeCharacters(val);
                xml.writeEndElement(); // Data
            }
            xml.writeEndElement(); // Cell
        }
        xml.writeEndElement(); // Row
    }

    xml.writeEndElement(); // Table
    xml.writeEndElement(); // Worksheet
}

bool SpreadsheetModel::exportToExcel(const QString &filePath) {
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
    collectStyles(xml, styleToId, styleIdx);
    xml.writeEndElement(); // Styles

    exportWorksheetToXml(xml, "Sheet1", styleToId, styleIdx);

    xml.writeEndElement(); // Workbook
    xml.writeEndDocument();
    outFile.close();

    if (QFile::exists(filePath)) {
        QFile::remove(filePath);
    }
    return QFile::rename(tmpPath, filePath);
}

void SpreadsheetModel::pushUndo() {
    if (m_macroDepth > 0) return;
    m_undoStack.append(toJsonObject());
    if (m_undoStack.size() > 50) {
        m_undoStack.removeFirst();
    }
    m_redoStack.clear();
}

void SpreadsheetModel::undo() {
    if (m_undoStack.isEmpty()) return;
    m_redoStack.append(toJsonObject());
    QJsonObject state = m_undoStack.takeLast();
    fromJsonObject(state);
}

void SpreadsheetModel::redo() {
    if (m_redoStack.isEmpty()) return;
    m_undoStack.append(toJsonObject());
    QJsonObject state = m_redoStack.takeLast();
    fromJsonObject(state);
}

void SpreadsheetModel::beginMacro() {
    if (m_macroDepth == 0) {
        pushUndo();
    }
    m_macroDepth++;
}

void SpreadsheetModel::endMacro() {
    if (m_macroDepth > 0) {
        m_macroDepth--;
    }
}

void SpreadsheetModel::clearUndoStack() {
    m_undoStack.clear();
    m_redoStack.clear();
    m_macroDepth = 0;
}

void SpreadsheetModel::getUsedRange(int &minR, int &minC, int &maxR, int &maxC) const {
    minR = 0;
    minC = 0;
    maxR = 19; // 기본 최소 20행
    maxC = 6;  // 기본 최소 7열 (G열)

    for (auto it = m_editedData.begin(); it != m_editedData.end(); ++it) {
        if (it.key().first > maxR) maxR = it.key().first;
        if (it.key().second > maxC) maxC = it.key().second;
    }
    for (auto it = m_fonts.begin(); it != m_fonts.end(); ++it) {
        if (it.key().first > maxR) maxR = it.key().first;
        if (it.key().second > maxC) maxC = it.key().second;
    }
    for (auto it = m_bgColors.begin(); it != m_bgColors.end(); ++it) {
        if (it.key().first > maxR) maxR = it.key().first;
        if (it.key().second > maxC) maxC = it.key().second;
    }
    for (auto it = m_fgColors.begin(); it != m_fgColors.end(); ++it) {
        if (it.key().first > maxR) maxR = it.key().first;
        if (it.key().second > maxC) maxC = it.key().second;
    }
    for (auto it = m_alignments.begin(); it != m_alignments.end(); ++it) {
        if (it.key().first > maxR) maxR = it.key().first;
        if (it.key().second > maxC) maxC = it.key().second;
    }
    for (const QRect &r : m_mergedRanges) {
        if (r.bottom() > maxR) maxR = r.bottom();
        if (r.right() > maxC) maxC = r.right();
    }
    maxR = qMin(maxR, rowCount() - 1);
    maxC = qMin(maxC, columnCount() - 1);
}

void SpreadsheetModel::applyVerticalText(const QModelIndexList &indexes, bool enable) {
    if (indexes.isEmpty()) return;
    pushUndo();
    
    QSet<QPair<int, int>> posSet = expandToMergedPositions(indexes);
    for (const auto &pos : posSet) {
        if (enable) m_verticalTexts[pos] = true;
        else m_verticalTexts.remove(pos);
    }
    
    int minR = indexes.first().row(), maxR = minR;
    int minC = indexes.first().column(), maxC = minC;
    for (const QModelIndex &idx : indexes) {
        if (idx.row() < minR) minR = idx.row();
        if (idx.row() > maxR) maxR = idx.row();
        if (idx.column() < minC) minC = idx.column();
        if (idx.column() > maxC) maxC = idx.column();
    }
    emit dataChanged(index(minR, minC), index(maxR, maxC), {Qt::UserRole + 4});
}

void SpreadsheetModel::applyFormat(const QModelIndexList &indexes, const QString &format) {
    if (indexes.isEmpty()) return;
    pushUndo();
    
    QSet<QPair<int, int>> posSet = expandToMergedPositions(indexes);
    for (const auto &pos : posSet) {
        if (format.isEmpty() || format == "General") {
            m_formats.remove(pos);
        } else {
            m_formats[pos] = format;
        }
    }
    
    int minR = indexes.first().row(), maxR = minR;
    int minC = indexes.first().column(), maxC = minC;
    for (const QModelIndex &idx : indexes) {
        if (idx.row() < minR) minR = idx.row();
        if (idx.row() > maxR) maxR = idx.row();
        if (idx.column() < minC) minC = idx.column();
        if (idx.column() > maxC) maxC = idx.column();
    }
    emit dataChanged(index(minR, minC), index(maxR, maxC), {Qt::DisplayRole});
}

static QString shiftFormula(const QString &formula, int rowDelta, int colDelta) {
    if (!formula.startsWith("=")) return formula;
    
    QString result = formula;
    QRegularExpression rx("(^|[^A-Za-z0-9_])((?:(?:'[^']+'|[^!+*/(),<>=&^%\\s]+)!)?)(\\$?)([A-Z]+)(\\$?)([1-9][0-9]*)(?![A-Za-z0-9_]|\\()");
    
    int offset = 0;
    QRegularExpressionMatch match;
    while ((match = rx.match(result, offset)).hasMatch()) {
        QString pre = match.captured(1);
        QString sheet = match.captured(2);
        QString colAbs = match.captured(3);
        QString colStr = match.captured(4);
        QString rowAbs = match.captured(5);
        QString rowStr = match.captured(6);
        
        int col = 0;
        for (QChar c : colStr) {
            col = col * 26 + (c.unicode() - 'A' + 1);
        }
        col -= 1;
        int row = rowStr.toInt() - 1;
        
        if (colAbs.isEmpty()) {
            col += colDelta;
            if (col < 0) col = 0;
        }
        if (rowAbs.isEmpty()) {
            row += rowDelta;
            if (row < 0) row = 0;
        }
        
        QString newColStr;
        int tempCol = col + 1;
        while (tempCol > 0) {
            int rem = (tempCol - 1) % 26;
            newColStr.prepend(QChar('A' + rem));
            tempCol = (tempCol - 1) / 26;
        }
        QString newRowStr = QString::number(row + 1);
        
        QString newRef = pre + sheet + colAbs + newColStr + rowAbs + newRowStr;
        result.replace(match.capturedStart(0), match.capturedLength(0), newRef);
        offset = match.capturedStart(0) + newRef.length();
    }
    
    return result;
}

void SpreadsheetModel::applyFillDown(const QModelIndexList &indexes) {
    if (indexes.isEmpty()) return;
    
    int minRow = rowCount();
    int maxRow = -1;
    int minCol = columnCount();
    int maxCol = -1;
    
    QSet<QPair<int, int>> posSet = expandToMergedPositions(indexes);
    if (posSet.isEmpty()) return;
    
    for (const auto &pos : posSet) {
        if (pos.first < minRow) minRow = pos.first;
        if (pos.first > maxRow) maxRow = pos.first;
        if (pos.second < minCol) minCol = pos.second;
        if (pos.second > maxCol) maxCol = pos.second;
    }
    
    pushUndo();
    
    if (minRow == maxRow) {
        if (minRow > 0) {
            for (int c = minCol; c <= maxCol; ++c) {
                QString val = getRawCellValue(minRow - 1, c);
                val = shiftFormula(val, 1, 0);
                setData(index(minRow, c), val, Qt::EditRole);
                m_formats[qMakePair(m_rowMap[minRow], c)] = m_formats.value(qMakePair(m_rowMap[minRow - 1], c), "");
                m_bgColors[qMakePair(m_rowMap[minRow], c)] = m_bgColors.value(qMakePair(m_rowMap[minRow - 1], c), QColor(Qt::white));
                m_fgColors[qMakePair(m_rowMap[minRow], c)] = m_fgColors.value(qMakePair(m_rowMap[minRow - 1], c), QColor(Qt::black));
                m_fonts[qMakePair(m_rowMap[minRow], c)] = m_fonts.value(qMakePair(m_rowMap[minRow - 1], c), QFont());
                m_alignments[qMakePair(m_rowMap[minRow], c)] = m_alignments.value(qMakePair(m_rowMap[minRow - 1], c), Qt::AlignLeft | Qt::AlignVCenter);
            }
        }
    } else {
        for (int c = minCol; c <= maxCol; ++c) {
            QString val = getRawCellValue(minRow, c);
            QString fmt = m_formats.value(qMakePair(m_rowMap[minRow], c), "");
            QColor bg = m_bgColors.value(qMakePair(m_rowMap[minRow], c), QColor(Qt::white));
            QColor fg = m_fgColors.value(qMakePair(m_rowMap[minRow], c), QColor(Qt::black));
            QFont font = m_fonts.value(qMakePair(m_rowMap[minRow], c), QFont());
            int align = m_alignments.value(qMakePair(m_rowMap[minRow], c), Qt::AlignLeft | Qt::AlignVCenter);
            
            for (int r = minRow + 1; r <= maxRow; ++r) {
                if (posSet.contains({r, c})) {
                    QString shiftedVal = shiftFormula(val, r - minRow, 0);
                    setData(index(r, c), shiftedVal, Qt::EditRole);
                    m_formats[qMakePair(m_rowMap[r], c)] = fmt;
                    m_bgColors[qMakePair(m_rowMap[r], c)] = bg;
                    m_fgColors[qMakePair(m_rowMap[r], c)] = fg;
                    m_fonts[qMakePair(m_rowMap[r], c)] = font;
                    m_alignments[qMakePair(m_rowMap[r], c)] = align;
                }
            }
        }
    }
}

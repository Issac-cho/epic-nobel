#pragma once
#include <QMainWindow>
#include "CustomTableView.h"
#include <QTabWidget>
#include <QComboBox>
#include <QPoint>
#include <QTreeWidget>
#include <QSplitter>

class SpreadsheetModel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() = default;

    void openFile(const QString &fileName);

    // 글로벌 수식 에디터 상태 추적 (어떤 탭에서도 접근 가능하도록)
    static QWidget* s_activeFormulaEditor;
    static QString s_formulaStartSheet;
    static QString s_formulaTextBeforeDrag;
    static bool s_isSelectingFormulaRange;
    
    // 시트 이름으로 모델 가져오기
    SpreadsheetModel* getSheetModel(const QString &sheetName);

private slots:
    void newDocument();
    void onOpenFileAction();
    void saveFile();
    void saveFileAs();
    void exportToExcel();
    void addNewSheet();
    void addNewFolder();
    void duplicateSheet(int index = -1);
    void deleteSheet(int index = -1);
    void renameSheet(int index = -1);
    void showTabContextMenu(const QPoint &pos);
    void showTreeContextMenu(const QPoint &pos);
    void onTreeItemClicked(QTreeWidgetItem *item, int column);
    void onTreeItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onTabChanged(int index);
    void applyBold();
    void applyItalic();
    void applyAlignLeft();
    void applyAlignCenter();
    void applyAlignRight();
    void applyBackgroundColor();
    void applyTextColor();
    void applyMerge();
    void applyFilter();
    void applyFind();
    void applyBorders(const QString &type);
    void changeFontSize(int size);
    void printSheet();

    void showContextMenu(const QPoint &pos);
    void insertRow();
    void deleteRow();
    void insertCol();
    void deleteCol();
    void applyCut();
    void applyCopy();
    void applyPaste();

private:
    void setupUi();
    void applyStyles();
    void restoreMergedRanges();
    CustomTableView* currentView() const;
    SpreadsheetModel* currentModel() const;
    CustomTableView* createSheetTab(SpreadsheetModel *model, const QString &title);
    bool saveProjectCx(const QString &filePath);
    bool loadProjectCx(const QString &filePath);
    bool exportProjectToExcel(const QString &filePath);
    void populateTreeFromTabs();
    void syncTreeToTabs();
    void buildTreeFromJson(const QJsonArray &treeArray);
    QJsonArray serializeTree() const;
    
    QSplitter *m_splitter;
    QTreeWidget *m_sheetTree;
    QTabWidget *sheetTabs;
    SpreadsheetModel *m_model;
    CustomTableView *m_tableView;
    QComboBox *m_fontSizeCombo;
    QString m_currentFilePath;
};

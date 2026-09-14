#pragma once

#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>

class MainWindowViewModel;
class MainWindowModel;
class TableWidget;
class Page1;
class Page2;
class Page3;
class Page4;
class Page5;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    Page1* page1() const { return m_page1; }
    Page2* page2() const { return m_page2; }
    Page3* page3() const { return m_page3; }
    Page4* page4() const { return m_page4; }
    Page5* page5() const { return m_page5; }

private slots:
    void onSaveConfig();
    void onSaveConfigAs();
    void onLoadConfig();
    void onRequestSaveDialog();
    void onRequestLoadDialog();
    void onShowMessage(const QString& title, const QString& message, int type);

private:
    MainWindowModel* m_model;
    MainWindowViewModel* m_viewModel;

    TableWidget* m_tableWidget = nullptr;   // 儲存指標供 run 鎖定使用

    // Pages - 初始化為 nullptr
    Page1* m_page1 = nullptr;
    Page2* m_page2 = nullptr;
    Page3* m_page3 = nullptr;
    Page4* m_page4 = nullptr;
    Page5* m_page5 = nullptr;

    void updateControlPageLock();
    void setupUI();
    void setupMenuBar();
    void setupConnections();
    void setupMessageService();
};

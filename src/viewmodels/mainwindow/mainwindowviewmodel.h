#pragma once

#include <QObject>
#include <QString>
#include "tablewidget.h"

class MainWindowModel;
class Page1;
class Page2;
class Page3;
class Page4;
class Page1ViewModel;
class Page2ViewModel;
class Page3ViewModel;
class Page4ViewModel;

class MainWindowViewModel : public QObject {
    Q_OBJECT

public:
    explicit MainWindowViewModel(MainWindowModel* model, QObject* parent = nullptr);
    ~MainWindowViewModel();

    // UI Widget 存取
    TableWidget* getMainWidget() const { return m_mainWidget; }

    // 檔案操作
    void saveConfig();
    void saveConfigAs();
    void loadConfig();

signals:
    void requestSaveDialog();
    void requestLoadDialog();
    void showMessage(const QString& title, const QString& message, int type);

public slots:
    void onSaveDialogAccepted(const QString& fileName);
    void onLoadDialogAccepted(const QString& fileName);

private:
    MainWindowModel* m_model;
    TableWidget* m_mainWidget;

    // Pages
    Page1* m_page1;
    Page2* m_page2;
    Page3* m_page3;
    Page4* m_page4;

    // 子 ViewModels
    Page1ViewModel* m_page1ViewModel;
    Page2ViewModel* m_page2ViewModel;
    Page3ViewModel* m_page3ViewModel;
    Page4ViewModel* m_page4ViewModel;

    void initializePages();
    void initializeViewModels();
    void initializeMainWidget();
    void setupPageConnections();
};

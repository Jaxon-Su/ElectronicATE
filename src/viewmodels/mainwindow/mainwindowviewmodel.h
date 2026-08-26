#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include "ixmlserializable.h"

class MainWindowModel;
class Page1ViewModel;
class Page2ViewModel;
class Page3ViewModel;
class Page4ViewModel;
class Page5ViewModel;

class MainWindowViewModel : public QObject {
    Q_OBJECT

public:
    explicit MainWindowViewModel(MainWindowModel* model, QObject* parent = nullptr);
    ~MainWindowViewModel();

    // Getter
    Page1ViewModel* page1ViewModel() const { return m_page1ViewModel; }
    Page2ViewModel* page2ViewModel() const { return m_page2ViewModel; }
    Page3ViewModel* page3ViewModel() const { return m_page3ViewModel; }
    Page4ViewModel* page4ViewModel() const { return m_page4ViewModel; }
    Page5ViewModel* page5ViewModel() const { return m_page5ViewModel; }

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

    // 子 ViewModels
    Page1ViewModel* m_page1ViewModel;
    Page2ViewModel* m_page2ViewModel;
    Page3ViewModel* m_page3ViewModel;
    Page4ViewModel* m_page4ViewModel;
    Page5ViewModel* m_page5ViewModel;

    void initializeViewModels();
    void setupPageConnections();
    QList<IXmlSerializable*> xmlPages() const;
};

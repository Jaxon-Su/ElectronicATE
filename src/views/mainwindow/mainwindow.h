#pragma once

#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>

class MainWindowViewModel;
class MainWindowModel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr); //MainWindow 設計為應用程式的主視窗
    ~MainWindow();

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

    void setupUI();
    void setupMenuBar();
    void setupConnections();
    void setupMessageService();
};

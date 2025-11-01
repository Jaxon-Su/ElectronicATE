#pragma once
#include <QObject>
#include <QString>

class MessageService : public QObject
{
    Q_OBJECT
public:
    //單例模式
    static MessageService& instance();

public slots:
    void showWarning(const QString& title, const QString& message);
    void showError(const QString& title, const QString& message);
    void showInfo(const QString& title, const QString& message);

signals:
    void warningRequested(const QString& title, const QString& message);
    void errorRequested(const QString& title, const QString& message);
    void infoRequested(const QString& title, const QString& message);

private:
    //私有建構函數在 private 區域，防止直接建立物件
    MessageService() = default;
    //刪除拷貝建構函數和賦值運算子
    MessageService(const MessageService& parameter) = delete;
    MessageService& operator=(const MessageService& parameter) = delete;
};

#pragma once

#include <QString>

// 前向宣告，不需要在這裡引入具體的實作標頭檔
class RelayBase;
class ICommunication;

class RelayFactory {
public:
    /**
     * @brief 根據型號名稱創建對應的 Relay 物件
     * @param modelName 型號名稱 (例如 "RTU_4")
     * @param comm 通訊介面指標
     * @return RelayBase* 成功則返回物件指標，失敗返回 nullptr
     */
    static RelayBase* createRelay(const QString& modelName, ICommunication* comm, quint8 slaveAddr = 0x01);
};

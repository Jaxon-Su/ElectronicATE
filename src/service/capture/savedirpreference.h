#pragma once
#include <QString>

// 跨 ViewModel 共用的「上次儲存目錄」持久化工具
// 任何需要記住存檔路徑的命令皆可使用，不依賴特定 ViewModel
class SaveDirPreference {
public:
    static QString load();
    static void    save(const QString& filePath);

private:
    static constexpr const char* kOrg     = "YourCompany";
    static constexpr const char* kApp     = "ElectronicATE";
    static constexpr const char* kKey     = "capture/lastSaveDir";
};

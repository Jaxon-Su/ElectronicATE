#pragma once
#include <QString>

class QXmlStreamWriter;
class QXmlStreamReader;

// 所有需要 XML 儲存/讀取的 ViewModel 實作此介面
// AppService 透過此介面操作，不需知道各 Page 的具體型別
class IXmlSerializable {
public:
    virtual ~IXmlSerializable() = default;

    // 回傳此 ViewModel 對應的 XML 元素名稱（如 "Page1"）
    virtual QString xmlTagName() const = 0;

    virtual void writeXml(QXmlStreamWriter& writer) const = 0;
    virtual void loadXml(QXmlStreamReader& reader)        = 0;
};

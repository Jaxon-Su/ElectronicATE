# Page1–Page3 XML 格式

三頁都必須帶有 `schemaVersion="2"`。缺少版本或版本不符會拒絕載入，不遷移舊存檔。

- Page1：固定名稱 `DC Source1`、`DC Source2`、`DC Source3`；通訊設定只使用 `CommunicationConfig`，不再讀寫舊 `Address` 元素，也不自動重新命名或補建舊 DC Source。
- Page2：`DcConditions` 包含多個 `Condition`，每組以 `name` 保存使用者名稱，必須有三個唯一的 `Source index="1|2|3"`，各保存 `Vin`、`CurrentLimit`。不再使用分台的 `DcTable` 或分頁狀態。
- Page3：`CurrentSelections/DcGroupSelection` 保存單一群組的 `index` 與 `text`；不保存實體輸出 ON 狀態。下拉名稱由 Page2 條件更新。

```xml
<Page2 schemaVersion="2">
  <DcConditions>
    <Condition name="額定輸入">
      <Source index="1"><Vin>24</Vin><CurrentLimit>5</CurrentLimit></Source>
      <Source index="2"><Vin>12</Vin><CurrentLimit>3</CurrentLimit></Source>
      <Source index="3"><Vin>5</Vin><CurrentLimit>1</CurrentLimit></Source>
    </Condition>
  </DcConditions>
</Page2>
```

Seq 右側的 DC Input 欄可直接編輯整組名稱；Parameter 欄標示 Vin／I Limit。

Page1 的 `DcInputs` 保存 DC Input 數量（1～3，預設 1）。Page2 僅顯示 Index1 至 IndexN；縮減數量保留隱藏欄位的設定。Page3 群組操作僅包含數量範圍內且已啟用的 DC Source。

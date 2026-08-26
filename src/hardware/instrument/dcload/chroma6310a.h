#pragma once
#include "chroma6310.h"

// ─────────────────────────────────────────────────────────────────────────
//  Chroma6310A
//
//  繼承 Chroma6310，差異如下：
//
//  1. setLoadMode() — 新增 CPL / CPH / LEDL / LEDH
//       CPL / CPH  : 定功率低檔 / 高檔
//       LEDL / LEDH: LED 模擬低壓 / 高壓（限 63110A, 63113A, 63115A, 63123A）
//
//  2. setStaticCurrent() / setDynamicCurrent()
//       改用 selectOptimalLoadMode6310A() 查詢 6310A 規格表
//
//  3. model() 回傳 "6310A"
//
//  SCPI 命令集與 6310 完全相同（CHAN、LOAD、CURR:STAT/DYN…）。
//
//  支援型號（subModel 傳入字串，需帶 A 後綴）：
//    63101A, 63102A, 63103A, 63105A, 63106A, 63107A,
//    63108A, 63110A, 63112A, 63113A, 63115A, 63123A
// ─────────────────────────────────────────────────────────────────────────

class Chroma6310A : public Chroma6310
{
public:
    // subModel: 如 "63103A"、"63123A"
    explicit Chroma6310A(const QString& subModel, ICommunication* comm = nullptr)
        : Chroma6310(subModel, comm) {}

    ~Chroma6310A() override = default;

    // ── 覆寫 mode 驗證，加入 CPL/CPH/LEDL/LEDH ──────────────────────────
    void setLoadMode(const QString& mode) override;

    // ── 覆寫 setStaticCurrent/setDynamicCurrent，改用 6310A spec 查詢 ───
    void setStaticCurrent(const StaticCurrentParam& param) override;
    void setDynamicCurrent(const DynamicCurrentParam& param) override;

    // ── 識別 ─────────────────────────────────────────────────────────────
    QString model()  const override { return "6310A"; }
    QString vendor() const override { return "Chroma"; }
};

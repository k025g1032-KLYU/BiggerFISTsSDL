# 執行中新增、移動與移除 Target

這一步建立敵人與投射物移植前需要的模型實例生命週期。一般的 `--first-person-preview` 仍以原本兩個靜態 Target 開始。

`FirstPersonGame` 保存 Target 的玩法狀態。`SpawnTarget` 用不重複的 `target_*` ID 加入 Target，`MoveTarget` 改變其位置與碰撞位置，`RemoveTarget` 將其移除。這些方法不使用 SDL 或 GPU 型別。按 R 重置時，會恢復場景起始的 Target 清單與位置，並清除執行中新增的 Target。

`FirstPersonScenePresenter::Sync` 會移除不存在或已被擊毀的 Target 模型、更新現存模型的位置，並為新 Target 建立模型實例。新增的 Target 共用啟動時已載入的 `target` mesh 與貼圖。`MeshRenderer` 每幀能處理不同數量的實例。此步驟**尚未**支援遊戲中臨時載入新的 OBJ 或 PNG；新增其他敵人外觀時，需要先在場景啟動時載入其 mesh。

使用實際 GPU 繪圖驗證：

```powershell
& '.\build\Debug\BiggerFISTsSDL.exe' --dynamic-target-smoke-test
```

此測試使用第一人稱場景：繪製五幀後加入 `target_dynamic`，接著移動，至第 25 幀移除，總共繪製 30 幀。每幀繪製前會檢查模型數量與位置。成功時輸出 `PASS: Dynamic Target spawned, moved, removed and rendered across 30 GPU frames`，結束碼為 0。

`FirstPersonGameTests` 另外檢查重複 ID、無效生命值、新增、移動、移除、共用 mesh、繪圖變換矩陣及重置。此步驟尚未加入敵人 AI、波次或投射物模型。

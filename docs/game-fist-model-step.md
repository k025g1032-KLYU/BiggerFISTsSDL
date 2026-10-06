# 載入原版戰鬥場景實際使用的左拳

原版 `KamataEngine20260622/DirectXGame/Game/CombatScene.cpp` 建立左拳時呼叫 `Model::CreateFromOBJ("LfistTEST")`。先前複製到 SDL 專案的 `LFist/Lfist.obj` 是另一份資源，這個戰鬥場景目前沒有使用它。本階段不刪除或修改舊資源，也不改使用者的 `config/model.cfg`。

## 三個檔案怎麼進入新專案

從原版 `Resources/LfistTEST/` 原樣複製：

```text
assets/models/LfistTEST/
├─ LfistTEST.obj
├─ LfistTEST.mtl
└─ Lfist.png
```

建置 `BiggerFISTsSDL` 時，CMake 的 `ModelAssets` 再把它們複製到 `build/Debug/assets/models/LfistTEST/`；Release 則放在 `build/Release/assets/models/LfistTEST/`。執行檔使用 `SDL_GetBasePath()` 尋找自己旁邊的副本，沒有向原版 KamataEngine 目錄讀檔。三個新副本已以檔案雜湊與原版比對，內容一致。

## 為什麼原本的 OBJ 載入器會拒絕

`LfistTEST.obj` 先用 `usemtl 材質`，之後改用 `usemtl 材質.001`。先前的載入器只接受一個材質名稱，因此遇到第二個名稱就報錯。但 `LfistTEST.mtl` 中兩個材質都寫著 `map_Kd Lfist.png`，在目前「不計算材質顏色與光照、只有一張不透明 PNG」的 renderer 中，可以使用同一份 GPU texture。

現在 `ObjLoader` 記錄真正有面的材質名稱，從 MTL 查出每個名稱的 `map_Kd`，確認它們解析到**同一個檔案**後才建立 `MeshData`。如果未來遇到兩種材質分別使用不同 PNG，載入會失敗並報告多貼圖尚未支援；程式不會只畫第一張而讓畫面悄悄出錯。

因此這次支援的是「多個材質名稱共用一張貼圖」，**不是**完整的多材質繪圖。MTL 的其他顏色、反射和光照參數仍未進入 GPU；材質使用不同貼圖時，日後需要分成多個 draw group 或材質批次。

## 為什麼還要調整預覽相機

`LfistTEST.obj` 的原點靠近拳頭一端，幾何範圍沿 Z 方向偏離原點。舊單模型檢視範例把相機固定在 `(0, 0, -3)` 並繞原點旋轉，斜角畫面因此裁掉部分前臂。

`TryConfigureModelPreview()` 現在掃過載入的頂點，算出最小／最大座標、包圍盒中心與最長的半邊長。它只把中心存入 `ModelScene` 作為**檢視時的旋轉中心**，並依大小後退相機；OBJ 頂點、原版檔案、原版遊戲中物件的座標都沒有修改。`Game` 從這個預覽場景起步，WASD 與旋轉行為仍維持原來的示範流程。

這個功能只是在 SDL 專案目前的模型檢視器裡方便確認資源。未來真正移植玩家手臂的物件階層時，應使用原版模型的局部原點和遊戲中的 WorldTransform，而不是把預覽中心當成遊戲座標。

## 目前如何驗證

在不改專案 `config/model.cfg` 的情況下，已暫時將 `build/Debug/model.cfg` 指向 `LfistTEST/LfistTEST.obj` 做一次 GPU smoke test，完成後恢復原內容。輸出顯示：

```text
Loaded model LfistTEST/LfistTEST.obj: 14132 vertices, 5192 triangles
Index upload submitted: 15576 indices, 62304 bytes (32-bit)
PNG upload submitted: .../assets/models/LfistTEST/Lfist.png (1024 x 1024, RGBA8)
PASS: BiggerFISTsSDL rendered 30 3D frames
```

另用離屏 GPU 渲染檢查 `build/texture-validation/lfist-test-angled.png`，已能看到完整的白黑拳臂，不再被相機裁掉。`ObjLoaderTests` 讀取原版 5,192 個三角形，也測試兩個材質共用 PNG 可以成功、不同 PNG 必須失敗。所有六項 CTest 測試通過。

若之後你想在 Visual Studio 自己預覽這份模型，可將專案的 `config/model.cfg` 設為 `model LfistTEST/LfistTEST.obj`、重新建置 `BiggerFISTsSDL`、按 F5。這只是切換單模型檢視範例，尚未把原版玩家戰鬥邏輯移植過來。

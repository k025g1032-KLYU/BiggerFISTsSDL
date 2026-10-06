# 將多模型場景接進主程式

`BiggerFISTsSDL` 現在有兩種啟動方式。直接啟動時仍讀 `model.cfg`，顯示一個可用 WASD 移動的模型；加上 `--scene-preview` 時讀 `scene.cfg`，顯示多個模型。這個選項是預覽開關，尚未代表正式遊戲流程。

## 如何執行

在專案根目錄開啟 PowerShell，先建置，再執行其中一種模式：

```powershell
$cmakeExe = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmakeExe --build build --config Debug --target BiggerFISTsSDL

& '.\build\Debug\BiggerFISTsSDL.exe'
& '.\build\Debug\BiggerFISTsSDL.exe' --scene-preview
```

上面兩個執行指令請分別使用；第一個視窗要先關閉，PowerShell 才會繼續下一行。不帶參數會讀 `build/Debug/model.cfg`；`--scene-preview` 會讀 `build/Debug/scene.cfg`。兩個設定檔都從專案的 `config/` 同步而來。預設 `scene.cfg` 會顯示 Target 和 LfistTEST。

在 Visual Studio 開啟 `build/BiggerFISTsSDL.slnx`，把 `BiggerFISTsSDL` 設為啟始專案。直接按 F5 是單模型模式；要進入多模型模式，在 `BiggerFISTsSDL` 專案的「屬性／偵錯／命令引數」填入 `--scene-preview` 後按 F5。若你的 Visual Studio 介面找不到這個欄位，使用上面的 PowerShell 指令即可。`--smoke-test` 可與 `--scene-preview` 同時使用，兩者順序皆可；測試會渲染 30 幀後自動離開。

## 程式執行時做了甚麼

1. `main.cpp` 讀取啟動參數。`--scene-preview` 決定讀哪份設定；無效或重複的參數會顯示用法並結束。
2. `Application.cpp` 建立 SDL 視窗。預設模式走既有的 `ModelConfig`／`ObjLoader` 單模型路徑。場景預覽模式則用 `SceneConfig` 讀取相機、模型清單和每個模型的 transform。
3. `SceneLoader.cpp` 是兩個多模型執行目標共用的載入器：依 `scene.cfg` 找到 OBJ，計算各模型的預覽中心，建立 `ModelScene`。同一份 OBJ 即使出現多次，也只建立一份 `MeshData`，各個 `ModelInstance` 以 `meshIndex` 指向它；`id` 則用來辨認場景中的個別物件。
4. `MeshRenderer` 將各份 mesh 和 PNG 上傳 GPU。主迴圈每幀更新旋轉時間，再把 `ModelScene` 交給 renderer 繪製。

預設單模型模式仍可用 WASD 移動；場景預覽模式暫時停用 WASD，因為設定檔還沒有指定哪個模型是玩家。兩種模式都沿用原本的退出、背景與顯示設定。模型資料仍從 SDL 專案內複製的 `assets/models/` 讀取；不會在執行時讀 KamataEngine 資料夾。

## 驗證結果與邊界

- `BiggerFISTsSDL --smoke-test`：原本的 `model.cfg` 選擇仍可渲染 30 幀。
- `BiggerFISTsSDL --scene-preview --smoke-test`：讀入 2 個 instance、2 份 mesh，成功渲染 30 幀。
- `MultiModelPreview --smoke-test`：獨立範例仍成功渲染 30 幀。
- `SceneLoaderTests` 檢查 3 個 instance 重用 2 份 mesh，以及模型檔缺失時不留下部分載入結果；全部 8 項 CTest 通過。

這一步把**顯示場景**接進主程式。每個場景物件現已有唯一 `id`，但尚未指定哪個是玩家，也沒有接入玩家控制、碰撞或戰鬥邏輯；詳見 [場景物件 ID 教學](scene-object-id-step.md)。`config/model.cfg` 和原版 KamataEngine 專案沒有修改。

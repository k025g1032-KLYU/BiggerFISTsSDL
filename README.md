# BiggerFISTs SDL3

這是將原本 PC 遊戲逐步移出 KamataEngine 的獨立 SDL3 專案。目前的 `BiggerFISTsSDL` 已使用 SDL GPU 顯示可用 WASD 移動的旋轉 3D 立方體。原版專案與 `vendor/SDL` 沒有被修改。

## SDL 版本鎖定

本專案使用 Git submodule `vendor/SDL`，主專案固定記錄 SDL **3.4.16** 的 commit `fa2c02bb6e21974a89ea9824bc53c9932abe5f9c`。CMake 直接建置這份原始碼，不會在建置時抓取最新版；設定階段也會驗證 checkout commit 與 SDL 版本，不符就停止。

重新取得專案時，使用 `git clone --recurse-submodules <repository-url>`；若已 clone 但缺少 `vendor/SDL`，在專案根目錄執行：

```powershell
git submodule update --init --recursive
```

GitHub 的一般「Download ZIP」不會包含 submodule 內容，直接拿 ZIP 建置會收到缺少 SDL 的錯誤。`docs/SDL_Download.txt` 已改為目前的下載說明；早期教學紀錄中的版本敘述僅代表當時的步驟。未來要升級 SDL 時，必須刻意更新 submodule commit 與 CMake 的版本鎖定值。

## 執行目標

| 目標 | 用途 |
|---|---|
| `BiggerFISTsSDL` | 主程式：輸入、遊戲狀態、計時、顯示設定和 3D 繪圖已接在同一個迴圈 |
| `GpuTriangle` | 獨立 GPU 範例及顯示切換檢查；和主程式共用 `CubeRenderer` |
| `InputActionsTests`、`WindowSettingsTests`、`MathTypesTests`、`GameTests` | 回歸測試 |

在 Visual Studio 開啟 `build/BiggerFISTsSDL.slnx`，將 `BiggerFISTsSDL` 設為啟始專案，選擇 Debug／x64，建置後按 F5。若先前已開啟解決方案，CMake 重新產生檔案後請重新載入。`GpuTriangle` 仍可獨立執行。

## 操作

| 按鍵 | 功能 |
|---|---|
| W／A／S／D | 在目前固定相機的 XY 平面移動立方體；W 對應世界正 Y |
| Space | 切換深色／暖色背景 |
| F1／F2 | 切換 1280×720／1600×900 視窗大小 |
| F11 | 切換無邊框全螢幕 |
| F8 | 移至下一個顯示螢幕；只有一個螢幕時不改變 |
| Esc | 結束程式 |

`config/gameplay.cfg` 的 `moveSpeed` 現在使用**世界單位／秒**，預設 `2.4`。`maxMovementDeltaTime` 只限制單幀移動量；旋轉仍使用實際經過時間。`config/input.cfg` 與 `config/display.cfg` 設定按鍵和顯示模式。CMake 會將設定檔放到執行檔旁邊。

## 原始碼分類

```text
src/
├─ App/         main、SDL 初始化及主迴圈
├─ Data/        遊戲設定定義與載入
├─ Game/        從遊戲動作更新場景
├─ Input/       按鍵狀態與動作映射
├─ Math/        向量與矩陣
├─ Platform/    視窗／顯示設定與計時
├─ Rendering/   GPU Device、Pipeline、Buffer、Depth Buffer 與繪製
├─ Samples/     獨立範例 GpuTriangle
└─ World/       立方體與固定相機的場景狀態
```

`App/Application.cpp` 讀取設定、處理 SDL 事件與輸入，並用 `Game::Update()` 更新場景，再呼叫 `CubeRenderer::Render()`。`CubeRenderer` 初始化 GPU、載入 shader、上傳立方體頂點／索引、建立深度貼圖，並依 backbuffer 實際尺寸建立 World × View × Projection 矩陣。主程式與範例都使用這份繪圖程式。尚未有功能的 `Core`、`Gameplay`、`Collision` 等目錄，等實際程式加入時再建立。

目前的 3D 內容仍是立方體教學場景；相機固定，模型載入、材質／光照和原作戰鬥內容尚未接入。這個專案目前使用 Windows Direct3D 12 與 DXIL shader。

## 建置與驗證

```powershell
$cmakeExe = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmakeExe -S . -B build -G 'Visual Studio 18 2026' -A x64
& $cmakeExe --build build --config Debug --target BiggerFISTsSDL GpuTriangle InputActionsTests WindowSettingsTests MathTypesTests GameTests
& (Join-Path (Split-Path -Parent $cmakeExe) 'ctest.exe') --test-dir build -C Debug --output-on-failure
& '.\build\Debug\BiggerFISTsSDL.exe' --smoke-test
& '.\build\Debug\GpuTriangle.exe' --test-display-settings
```

`--smoke-test` 繪製 30 幀後自動結束，用於確認主程式 GPU 初始化和渲染。`--test-display-settings` 依序檢查視窗尺寸、全螢幕、最大化與最小化恢復。正式執行時不帶參數。

[先前的逐步教學紀錄](docs/tutorial-history.md)保留了白色方塊、GPU 三角形及立方體各階段的解說；其中早期檔案路徑和執行結果是當時的狀態，請以本 README 的目錄與操作為準。

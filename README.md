# BiggerFISTs SDL3

這是將原本 PC 遊戲逐步移出 KamataEngine 的獨立 SDL3 專案。`BiggerFISTsSDL` 預設依照 `config/model.cfg` 載入一個原版 OBJ 模型及其 PNG，可用 WASD 移動並旋轉預覽；加上 `--scene-preview` 時，則依 `config/scene.cfg` 顯示多個模型；加上 `--first-person-preview` 時，則可用原版移動規則與第一人稱相機接近 Target，操作左右拳蓄力、出拳並擊毀它。`MultiModelPreview` 保留為獨立多模型範例。專案已複製 Target、LFist、原版戰鬥場景使用的 LfistTEST 與 RFist。`GpuTriangle` 保留貼圖立方體範例。原版專案與 `vendor/SDL` 沒有被修改。

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
| `BiggerFISTsSDL` | 主程式：預設依 `model.cfg` 載入一個模型；`--scene-preview` 顯示多個模型；`--first-person-preview` 可用左右拳擊毀並重置 Target |
| `MultiModelPreview` | 依 `config/scene.cfg` 在同一畫面顯示多個模型，各自帶 PNG 貼圖並旋轉 |
| `GpuTriangle` | 貼圖立方體與顯示切換檢查；和主程式共用 `MeshRenderer` |
| `InputActionsTests`、`WindowSettingsTests`、`MathTypesTests`、`GameTests`、`FirstPersonGameTests`、`CollisionTests`、`ObjLoaderTests`、`ModelConfigTests`、`SceneConfigTests`、`SceneLoaderTests` | 回歸測試 |

在 Visual Studio 開啟 `build/BiggerFISTsSDL.slnx`，將 `BiggerFISTsSDL` 設為啟始專案，選擇 Debug／x64，建置後按 F5，會進入原本的單模型模式。要看第一人稱場景，將啟動參數設為 `--first-person-preview`；也可以從 PowerShell 執行 `& '.\build\Debug\BiggerFISTsSDL.exe' --first-person-preview`。多模型模式使用 `--scene-preview`。若先前已開啟解決方案，CMake 重新產生檔案後請重新載入。操作與檔案關係見 [第一人稱移植教學](docs/first-person-port-step.md)；原有預覽模式見 [主程式場景預覽教學](docs/main-scene-preview-step.md)。

## 操作

| 按鍵 | 功能 |
|---|---|
| W／A／S／D | 預設單模型模式：在固定相機的 XY 平面移動模型；第一人稱模式：依相機方向移動玩家 |
| 滑鼠移動 | 第一人稱模式：轉動視角 |
| Q／E | 第一人稱模式：按住蓄力左／右拳，蓄滿後放開出拳 |
| Shift＋滑鼠左／右鍵 | 第一人稱模式：以滑鼠蓄力左／右普通拳 |
| R | 第一人稱模式：重置 Target 與左右拳狀態 |
| Space | 僅主程式：切換深色／暖色背景 |
| F1／F2 | 切換 1280×720／1600×900 視窗大小 |
| F11 | 切換無邊框全螢幕 |
| F8 | 移至下一個顯示螢幕；只有一個螢幕時不改變 |
| Esc | 結束程式 |

`config/gameplay.cfg` 的 `moveSpeed` 用於**預設模型預覽模式**，預設 `2.4` 世界單位／秒。第一人稱移動數值目前集中在 `FirstPersonTuning.h`，採用原版的加速度和最高速度。`maxMovementDeltaTime` 限制單幀移動量。`config/input.cfg`、`config/display.cfg`、`config/model.cfg` 分別設定按鍵、顯示模式及主程式模型；`config/scene.cfg` 設定多模型預覽；`config/first-person-scene.cfg` 設定第一人稱模式的初始視點與 Target。CMake 會將設定檔放到執行檔旁邊。

## 原始碼分類

```text
src/
├─ App/         main、SDL 初始化及主迴圈
├─ Collision/   球體重疊與移動路徑碰撞判定
├─ Data/        遊戲設定、OBJ／MTL 與 mesh 資料
├─ Game/        從遊戲動作更新場景
├─ Gameplay/    已移植的玩家移動規則與集中數值
├─ Input/       按鍵狀態與動作映射
├─ Math/        向量與矩陣
├─ Platform/    視窗／顯示設定與計時
├─ Rendering/   GPU Device、Pipeline、Buffer、Depth Buffer 與繪製
├─ Samples/     獨立立方體與多模型範例
└─ World/       模型場景與第一人稱相機狀態
```

`App/Application.cpp` 預設使用 `ModelConfig` 選擇一個原版模型；場景預覽模式使用 `SceneConfig` 與共用的 `SceneLoader` 建立多模型場景；第一人稱模式載入靜態 Target 與左右拳，交由 `FirstPersonGame` 更新玩家、相機、拳頭狀態與 Target 命中。各模式最後都呼叫 `MeshRenderer::Render()`。`ModelScene` 保存多個 `ModelInstance`；每個 instance 指向一份 mesh，並有自己的位置、中心與旋轉。`MeshRenderer` 上傳各 mesh 的頂點、索引和貼圖，在同一個 render pass 中逐一繪製，並共用深度貼圖。`GpuTriangle` 將內建的立方體 mesh 傳給同一個 renderer。尚未有功能的 `Core` 等目錄，等實際程式加入時再建立。

主程式的預設模式一次選擇一個模型，場景預覽模式則從 `scene.cfg` 顯示多個模型；每個場景物件有唯一 `id`，多模型預覽中 WASD 暫不移動任何模型。預覽會依模型頂點範圍調整旋轉中心，不修改 OBJ 頂點。光照和原作戰鬥內容尚未接入。這個專案目前使用 Windows Direct3D 12 與 DXIL shader。`scene.cfg` 的設定格式見 [同畫面多模型教學](docs/multi-model-step.md)，ID 的用途見 [場景物件 ID 教學](docs/scene-object-id-step.md)。

## 選擇原版模型

編輯專案中的 `config/model.cfg` 可以選擇模型，例如 `model Target/Target.obj` 或 `model LfistTEST/LfistTEST.obj`。`LFist/Lfist.obj` 是另一份未由目前原版戰鬥場景使用的資源，仍保留其複本；本次沒有修改使用者目前的選擇。模型路徑從執行檔旁的 `assets/models/` 起算。CMake 會同步設定檔與四組資源到 `build/Debug/`，因此請修改專案中的 `config/model.cfg`，不要只修改 `build/Debug/model.cfg`。選取不存在或不支援的模型時，程式會顯示錯誤並結束。

詳細步驟和設定檔、OBJ、MTL、PNG 的載入順序見 [模型選擇教學](docs/model-selection-step.md)。

原版 `CombatScene` 使用 `LfistTEST` 作為左拳。它的兩個材質名稱都指向同一張 PNG；載入器現在可檢查並載入這種情況。若材質使用不同貼圖，目前會明確報錯，避免只畫其中一張。實作與驗證見 [實際使用的左拳模型教學](docs/game-fist-model-step.md)。

## 原版 Target 模型

當 `model.cfg` 指向 Target 時，主程式從 `assets/models/Target/Target.obj` 載入模型，根據 `mtllib` 讀取 `Target.mtl`，再從 `map_Kd` 找到 `target.png`。OBJ 的三角形與四邊形會轉成 GPU 三角形；獨立的 v／vt／vn 索引會組合成可用的 GPU 頂點。模型、MTL 或貼圖檔有變更時，重新建置 `BiggerFISTsSDL` 即會同步到執行目錄。

詳見 [原版模型載入與執行教學](docs/obj-model-step.md)。

## PNG 貼圖

`GpuTriangle` 範例的 `assets/textures/cube.png` 是從原版 `Resources/uvChecker.png` 複製的 512×512 檢查圖。六個面各使用完整一張圖；24 個頂點各帶位置與 UV，36 個索引組成 12 個三角形。貼圖目前為不受光照影響的不透明顯示，PNG 的 Alpha 不參與透明混合。

`Rendering/TextureLoader.cpp` 使用固定版本 SDL 內建的 `SDL_LoadPNG()`，轉成 RGBA32 後逐列複製到 GPU transfer buffer，再上傳成 `SDL_GPUTexture`。只在初始化時讀圖與上傳；每幀共用同一張貼圖和 sampler。

更換範例立方體的 PNG 時，修改 `assets/textures/cube.png` 並重新建置 `GpuTriangle`；更換主程式 Target 貼圖時，修改 `assets/models/Target/target.png` 並重新建置 `BiggerFISTsSDL`。CMake 會同步到 `build/Debug/assets/`；Release 使用 `build/Release/`。程式從執行檔所在位置找資源，因此可以從 Visual Studio 或其他工作目錄啟動。

完整流程、UV 解說、資源生命週期及本次驗證結果見 [PNG 貼圖實作與執行教學](docs/png-texture-step.md)。

## 建置與驗證

```powershell
$cmakeExe = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmakeExe -S . -B build -G 'Visual Studio 18 2026' -A x64
& $cmakeExe --build build --config Debug --target BiggerFISTsSDL MultiModelPreview GpuTriangle InputActionsTests WindowSettingsTests MathTypesTests GameTests FirstPersonGameTests ObjLoaderTests ModelConfigTests SceneConfigTests SceneLoaderTests
& (Join-Path (Split-Path -Parent $cmakeExe) 'ctest.exe') --test-dir build -C Debug --output-on-failure
& '.\build\Debug\BiggerFISTsSDL.exe' --smoke-test
& '.\build\Debug\BiggerFISTsSDL.exe' --scene-preview --smoke-test
& '.\build\Debug\BiggerFISTsSDL.exe' --first-person-preview --smoke-test
& '.\build\Debug\MultiModelPreview.exe' --smoke-test
& '.\build\Debug\GpuTriangle.exe' --test-display-settings
```

`--smoke-test` 繪製 30 幀後自動結束，用於確認 GPU 初始化和渲染。`--test-display-settings` 依序檢查視窗尺寸、全螢幕、最大化與最小化恢復。正式執行時不帶參數；例如執行 `.\build\Debug\MultiModelPreview.exe` 可持續觀看兩個模型。

[先前的逐步教學紀錄](docs/tutorial-history.md)保留了白色方塊、GPU 三角形及立方體各階段的解說；其中早期檔案路徑和執行結果是當時的狀態，請以本 README 的目錄與操作為準。

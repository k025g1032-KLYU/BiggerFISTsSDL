# 用設定檔切換原版模型

這一步讓 `BiggerFISTsSDL` 的模型選擇由 `config/model.cfg` 決定。程式仍然一次顯示一個模型。本文記錄當時以 Target 和 LFist 驗證的步驟；後來確認原版戰鬥場景實際使用的左拳是 `LfistTEST`，並已加入支援，詳見 [實際使用的左拳模型教學](game-fist-model-step.md)。原版 KamataEngine 的資源沒有修改。

## 你可以怎麼操作

1. 開啟專案根目錄的 `config/model.cfg`。本階段建立時的預設內容為；之後使用者可能已改動：

   ```text
   # Relative to assets/models in the executable directory.
   model Target/Target.obj
   ```

2. 此階段曾以 `model LFist/Lfist.obj` 測試另一份模型；目前若要預覽原版戰鬥場景實際使用的左拳，請使用 `model LfistTEST/LfistTEST.obj`。
3. 在 Visual Studio 選擇 `Debug`／`x64`，建置 `BiggerFISTsSDL`，再按 F5。若專案提示重新載入，先重新載入。
4. 看 PowerShell 或 Visual Studio 啟動的終端輸出。若沿用當時的 LFist 範例，會看到 `Loaded model LFist/Lfist.obj: 14990 vertices, 4998 triangles`，接著看到 `Lfist.png` 的上傳訊息；這份貼圖主要是深紫色與黑色。若選實際使用的 `LfistTEST`，請以 [新教學](game-fist-model-step.md) 的紀錄為準。
5. 要恢復靶球，將該行改回 `model Target/Target.obj`，重新建置、執行。

修改的是**專案的** `config/model.cfg`。CMake 在建置時將設定產生到 `build/Debug/model.cfg`。只改 `build/Debug/model.cfg` 可以暫時測試，但下次重新產生建置檔時可能被覆蓋。Release 使用 `build/Release/model.cfg`。

PowerShell 完整建置與執行指令：

```powershell
Set-Location 'G:\class\GameCreator\EX\robot game 3D 001\BiggerFISTsSDL'
$cmakeExe = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmakeExe -S . -B build -G 'Visual Studio 18 2026' -A x64
& $cmakeExe --build build --config Debug --target BiggerFISTsSDL
& '.\build\Debug\BiggerFISTsSDL.exe'
```

## 程式如何使用這一行設定

```text
config/model.cfg
    ↓ CMake 放到執行檔旁
build/Debug/model.cfg
    ↓ ModelConfig 讀取「model LFist/Lfist.obj」
build/Debug/assets/models/LFist/Lfist.obj
    ↓ ObjLoader 讀取 mtllib，再找 Lfist.mtl
build/Debug/assets/models/LFist/Lfist.mtl
    ↓ ObjLoader 讀取 map_Kd，再找 Lfist.png
MeshData（頂點、索引、貼圖路徑）
    ↓ MeshRenderer 上傳 GPU 並繪製
```

程式用 `SDL_GetBasePath()` 找執行檔位置，因此從 Visual Studio 或不同工作目錄啟動，也會找同一個 `model.cfg`。`ModelConfig` 只接受相對的 `.obj` 路徑；不允許 `..` 或絕對路徑。檔案不存在、設定重複、材質使用不同 PNG 時，會印出錯誤而停止，不會悄悄換成其他模型。

`assets/models/Target/`、`assets/models/LFist/`、`assets/models/LfistTEST/` 都是從原版專案複製。CMake 的 `ModelAssets` 目標每次建置主程式時用 `copy_if_different` 同步三組 OBJ、MTL、PNG。`Game`、預覽相機、WASD 移動和 `MeshRenderer` 不需要知道模型的檔名。

## 這次改了哪些檔案

| 檔案 | 作用 |
|---|---|
| `config/model.cfg` | 選擇要啟動的 OBJ，預設 Target |
| `src/Data/ModelConfig.h/.cpp` | 讀設定、檢查格式與相對路徑 |
| `src/App/Application.cpp` | 使用設定選取 OBJ，並在輸出中報告名稱和三角形數 |
| `assets/models/LFist/` | 原版左拳的 OBJ、MTL、PNG 複本 |
| `CMakeLists.txt` | 複製設定與資源、建立測試目標 |
| `tests/ModelConfigTests.cpp` | 檢查選取、UTF-8 BOM、路徑跳出與重複設定 |
| `tests/ObjLoaderTests.cpp` | 確認原版 LFist OBJ 與 PNG 確實能被讀取 |

## 驗證結果與界線

Debug 建置成功。六項 CTest 測試通過。主程式使用 Target 時輸出 1,984 個頂點、960 個三角形；切換 LFist 後輸出 14,990 個頂點、4,998 個三角形。兩種設定都成功上傳 PNG 並繪製 30 個影格。另用離屏 GPU 檢查產生 `build/texture-validation/lfist-front.png` 和 `lfist-angled.png`，確認左拳實際形狀與原版紫黑貼圖。

這一步是**啟動時選擇單一模型**。執行中熱切換、多個模型同時出現、光照與動畫都還沒有實作。之後若要顯示整個原版場景，需要讓 renderer 管理多份 mesh、材質和每個物件自己的 transform。

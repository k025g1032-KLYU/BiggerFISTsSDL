# 原版 Target 模型載入與執行教學

日期：2026-10-05。工作分支：`v0.2`。

後續已加入 `config/model.cfg`，現在可在 Target 與 LFist 間切換。本文記錄首次載入 Target 的實作步驟；目前模型選擇方式請看 [模型選擇教學](model-selection-step.md)。

再往後已加入原版戰鬥場景實際使用的 `LfistTEST`，可讀取多個材質名稱共用一張 PNG 的 OBJ；詳見 [實際使用的左拳模型教學](game-fist-model-step.md)。本文件下方的單材質限制記錄的是首次載入 Target 時的狀態。

本階段讓 `BiggerFISTsSDL` 從檔案讀取原版的 `Target.obj`、`Target.mtl` 和 `target.png`，在 SDL GPU 視窗顯示紅白靶球。它仍可使用 W/A/S/D 移動、F1/F2 改變大小、F11 切換全螢幕、F8 換螢幕、Space 改變背景、Esc 結束。

`GpuTriangle` 繼續顯示先前的貼圖立方體。兩個執行目標現在共用 `MeshRenderer`；差別在於傳入的 `MeshData` 來源。

## 在 Visual Studio 執行

1. 開啟 `build/BiggerFISTsSDL.slnx`。若 Visual Studio 提示重新載入 CMake 產生的專案，請重新載入。
2. 選擇 `Debug`、`x64`，設定 `BiggerFISTsSDL` 為啟始專案。
3. 建置後按 F5。畫面應顯示一顆緩慢旋轉、可移動的紅白靶球；視窗標題為 `BiggerFISTs SDL3 Target Model`。
4. 若要查看之前的立方體，將 `GpuTriangle` 設為啟始專案後執行。

PowerShell 可在 BiggerFISTsSDL 專案根目錄執行：

```powershell
$cmakeExe = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmakeExe -S . -B build -G 'Visual Studio 18 2026' -A x64
& $cmakeExe --build build --config Debug --target BiggerFISTsSDL GpuTriangle ObjLoaderTests
& '.\build\Debug\BiggerFISTsSDL.exe'
```

主程式啟動時應看到類似：

```text
Loaded Target.obj: 1984 vertices, 960 triangles
Vertex upload submitted: 1984 vertices, 39680 bytes
Index upload submitted: 2880 indices, 11520 bytes (32-bit)
PNG upload submitted: .../assets/models/Target/target.png (1024 x 1024, RGBA8)
```

這些訊息各自證明 CPU 已解析 OBJ、GPU 已接收頂點與索引、PNG 已送進 GPU。畫面正面的靶心與斜角時的球面貼圖也已透過 GPU 離屏繪製確認。

## 檔案放在哪裡

原版檔案已複製到 SDL 專案，沒有修改原版：

```text
assets/models/Target/
├─ Target.obj
├─ Target.mtl
└─ target.png
```

改動來源檔後，重新建置 `BiggerFISTsSDL`，`TargetModelAssets` 會把三個檔案同步到 `build/Debug/assets/models/Target/`。請修改專案 `assets/` 內的來源，因為 `build/` 是建置結果。

程式使用 `SDL_GetBasePath()` 找到執行檔資料夾，再接上 `assets/models/Target/Target.obj`。因此從 Visual Studio 或不同工作目錄執行，仍能找到資源。

## OBJ、MTL、PNG 的關係

OBJ 是模型的幾何資料。`Target.obj` 中重要的語法是：

```text
v  ...                  頂點位置 x, y, z
vt ...                  圖片座標 u, v
vn ...                  法線方向 x, y, z
f  478/1/1 6/2/1 ...  一個面；每角依序指定 v/vt/vn 索引
mtllib Target.mtl       材質檔名
usemtl 材質             後面的面使用哪一個材質
```

上面是閱讀示意；OBJ 的各欄位都是文字，以空白分隔。OBJ 索引從 **1** 開始，而且位置、UV、法線各有自己的索引。例如 `478/1/1` 表示第 478 個位置、第 1 個 UV、第 1 個法線。載入器檢查索引有效後，組成一個 GPU 頂點，並把同樣的組合重用。

`Target.mtl` 裡 `newmtl 材質` 定義材質名稱，`map_Kd target.png` 指定貼圖。載入器以 MTL 檔所在資料夾為起點解析 `target.png`；不需要把這個名字寫死在 C++ 中。

OBJ 的 V 座標以圖片底端為 0，而目前 SDL GPU 上傳的 PNG 以頂端為 0。載入器把 UV 的 V 轉成 `1.0f - v`。若忘記這一步，靶心圖案可能上下顛倒。

## 為什麼是 960 個三角形與 1,984 個 GPU 頂點

原版 `Target.obj` 有 64 個三角形面與 448 個四邊形面。GPU 繪圖管線在此使用三角形，所以每個四邊形拆成 2 個三角形：

```text
64 + 448 × 2 = 960 triangles
960 × 3 = 2880 indices
```

OBJ 中同一個位置可以在不同面使用不同 UV 或法線。一個 GPU 頂點卻只能有一組位置和 UV，所以載入器以完整的 `v/vt/vn` 組合作為重用條件。轉換後得到 1,984 個 GPU 頂點；這比 OBJ 的位置數多，是正常結果。

目前 fragment shader 直接顯示 PNG，不計算光照，因此讀入的法線只用於驗證 OBJ 索引並保持頂點組合正確。日後加入光照時，需要把法線也存入 `MeshVertex` 並傳到 shader。

## 程式資料如何流動

```text
Target.obj + Target.mtl
      ↓ ObjLoader：解析文字、檢查索引、拆四邊形、尋找 map_Kd
MeshData：頂點、三角形索引、PNG 路徑
      ↓ MeshRenderer：上傳 GPU buffers、載入 PNG 與 sampler
SDL GPU：每幀用同一份 GPU 資源繪製
```

`MeshData` 是 CPU 記憶體中的普通資料；`ObjLoader` 不需要 SDL 視窗或 GPU。`MeshRenderer` 接收這份資料，建立 GPU 頂點 buffer 與 32-bit 索引 buffer，並沿用上一階段的 PNG 上傳流程。模型與貼圖都只在初始化時載入，每幀繪製時不重新讀檔。

主程式在 `App/Application.cpp` 呼叫 `LoadObjModel()`，然後把結果交給 `MeshRenderer::Initialize()`。`GpuTriangle` 由 `Samples/SampleCubeMesh.cpp` 建立測試立方體，也傳入相同的 `MeshData`。這樣兩個程式共用 shader、深度貼圖、GPU 建立與釋放流程。

場景狀態集中於 `World/ModelScene.h/.cpp`，仍提供位置、旋轉角度、固定相機、背景顏色和 World × View × Projection 計算。先前叫 `CubeScene`，本次改名以反映它已能顯示其他模型。範例靶球的垂直移動範圍縮至 ±0.4 世界單位，減少移出固定相機畫面。

## 本次新增或修改的原始碼

| 路徑 | 用途 |
|---|---|
| `src/Data/MeshData.h` | CPU 頂點、索引、貼圖路徑 |
| `src/Data/ObjLoader.h/.cpp` | 解析 OBJ 和單一 MTL／PNG 材質 |
| `src/Rendering/MeshRenderer.h/.cpp` | GPU 頂點／索引／貼圖、繪製與清理；由 `CubeRenderer` 改名 |
| `src/Samples/SampleCubeMesh.h/.cpp` | 保留之前的立方體頂點範例 |
| `src/World/ModelScene.h/.cpp` | 模型與相機狀態；由 `CubeScene` 改名 |
| `src/App/Application.cpp` | 主程式載入 Target，顯示可理解的載入錯誤 |
| `src/Samples/GpuTriangle.cpp` | 範例傳入立方體 MeshData |
| `src/Game/Game.cpp` | 調整靶球在固定相機內的移動範圍 |
| `tests/ObjLoaderTests.cpp` | 原版模型與損壞索引測試 |
| `CMakeLists.txt` | 更新目標來源、測試與三個資源的複製規則 |

## 支援範圍與錯誤

這個小型載入器目前支援一個 `mtllib`、一個 `usemtl`、一張 `map_Kd` PNG、正數 `v/vt/vn` 索引、三角形與四邊形。若 OBJ 缺少必要資料、索引超出範圍、MTL 缺少貼圖，會停止載入並指出檔案或行數。這讓第一個原版模型有明確可驗證的範圍。

尚未支援多材質、負數 OBJ 索引、五邊形以上、MTL 貼圖選項、JPG、光照、透明混合或動畫。這些能力需要時再依實際模型逐項加入；不會因為某個 unsupported 格式而默默畫錯。

## 驗證

```powershell
& $cmakeExe --build build --config Debug --target BiggerFISTsSDL GpuTriangle InputActionsTests WindowSettingsTests MathTypesTests GameTests ObjLoaderTests
& (Join-Path (Split-Path -Parent $cmakeExe) 'ctest.exe') --test-dir build -C Debug --output-on-failure
& '.\build\Debug\BiggerFISTsSDL.exe' --smoke-test
& '.\build\Debug\GpuTriangle.exe' --test-display-settings
```

`ObjLoaderTests` 載入原版 Target，檢查 960 個三角形、貼圖路徑與索引有效性；另外檢查四邊形拆分、UV 翻轉及超出範圍索引的錯誤訊息。`--smoke-test` 在繪製 30 幀後自動結束。GPU 離屏檢查圖放在忽略提交的 `build/texture-validation/target-front.png` 和 `target-angled.png`，可查看靶心與球面貼圖方向。

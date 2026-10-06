# PNG 貼圖實作與執行教學

日期：2026-10-05。工作分支：`v0.2`。

本階段的成果是讓目前的 3D 立方體顯示真正從 PNG 檔案讀取的圖案。主程式 `BiggerFISTsSDL` 和獨立範例 `GpuTriangle` 共用這份功能。

後續加入原版 Target 模型後，主程式已改成顯示 Target；貼圖立方體保留在 `GpuTriangle`。本文件記錄當時的 PNG 教學步驟，其中 `CubeRenderer` 已改名為 `MeshRenderer`，`CubeScene` 已改名為 `ModelScene`。請以專案 README 與 [原版模型載入教學](obj-model-step.md) 的現況為準。

## 如何執行

1. 在 Visual Studio 開啟 `build/BiggerFISTsSDL.slnx`。如果出現重新載入專案提示，接受重新載入。
2. 選擇 `Debug` 與 `x64`，把 `BiggerFISTsSDL` 設定為啟始專案。
3. 建置後按 F5。畫面應顯示旋轉的彩色格子立方體，每面都有 `0,0` 到 `7,7` 的座標格。
4. W/A/S/D 移動立方體；F1/F2 更改視窗大小，F11 切換全螢幕，F8 切換螢幕，Esc 結束。

PowerShell 也可以執行；先切到 BiggerFISTsSDL 專案目錄：

```powershell
$cmakeExe = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmakeExe -S . -B build -G 'Visual Studio 18 2026' -A x64
& $cmakeExe --build build --config Debug --target BiggerFISTsSDL
& '.\build\Debug\BiggerFISTsSDL.exe'
```

## 如何換成自己的 PNG

修改或替換專案中的 `assets/textures/cube.png`，保留檔名。結束正在執行的程式，重新建置並啟動，就會看到新圖片。只修改圖片也可以：CMake 的 `CubeAssets` 目標每次建置都檢查並複製有變更的圖片。

不要只修改 `build/Debug/assets/textures/cube.png`；那是建置產物，下次建置會從專案的原始圖片覆蓋回來。原始 PNG 要和程式一起加入 Git。

圖片不必是 512×512，也不必是正方形，但尺寸需在 GPU 支援範圍內。每面都把整張圖映射到正方形，因此長方形圖片會被壓縮或拉伸。先使用不透明 PNG；本階段 shader 固定輸出 Alpha = 1，尚未實作透明材質。

圖片只在啟動時載入，沒有即時重新載入功能。重新啟動是必要的。

## 修改的檔案

| 檔案 | 責任 |
|---|---|
| `assets/textures/cube.png` | 測試圖；從原版 `Resources/uvChecker.png` 複製，原版檔案未修改 |
| `src/Rendering/TextureLoader.h/.cpp` | PNG 解碼、格式轉換、GPU 貼圖建立與上傳 |
| `src/Rendering/CubeRenderer.h/.cpp` | UV 頂點資料、持有貼圖及 sampler、繪製時綁定、結束時釋放 |
| `shaders/triangle.vert.hlsl` | 計算畫面位置，將 UV 傳到 fragment shader |
| `shaders/triangle.frag.hlsl` | 根據 UV 從 PNG 貼圖取色 |
| `CMakeLists.txt` | 編譯新增程式、編譯 shader、同步 PNG 到執行目錄 |
| `README.md` | 更新目前成果與執行說明 |

shader 檔名沿用早期三角形範例的 `triangle`，目前實際用於立方體。

## 程式啟動時做什麼

`Application::Run()` 建立 SDL 視窗後，呼叫 `CubeRenderer::Initialize()`。

1. 建立 GPU device、graphics pipeline、立方體頂點與索引 buffer。
2. 用 `SDL_GetBasePath()` 取得執行檔所在目錄，接上 `assets/textures/cube.png`，形成完整路徑。這避免依賴 PowerShell 或 Visual Studio 的目前工作目錄。
3. 呼叫 `LoadPngTexture()`。`SDL_LoadPNG()` 把 PNG 解碼成 CPU 記憶體裡的 `SDL_Surface`；這時還不是 GPU 貼圖。
4. `SDL_ConvertSurface(..., SDL_PIXELFORMAT_RGBA32)` 把像素統一成每像素 4 bytes，依序為紅、綠、藍、Alpha。
5. 建立 `SDL_GPUTexture` 和 upload transfer buffer。前者是 GPU 最終使用的貼圖；後者是把 CPU 像素送往 GPU 的暫存空間。
6. 透過 `SDL_MapGPUTransferBuffer()` 取得可寫入的記憶體，逐列複製圖片，再 unmap。
7. 在 copy pass 呼叫 `SDL_UploadToGPUTexture()`，提交 command buffer。
8. 建立 sampler，指定線性過濾和邊緣夾取；本階段只有一層貼圖，沒有 mipmaps。

可把初始化流程讀成：

```text
PNG file
  -> SDL_Surface (CPU pixels)
  -> RGBA32 pixels
  -> GPU transfer buffer
  -> SDL_GPUTexture
```

PNG 載入功能直接使用已固定的 SDL 3.4.16，沒有新增外部函式庫。

## UV 是什麼，為何改成 24 個頂點

位置 `position` 決定頂點在 3D 空間的位置；`uv` 決定這個頂點對應圖片的哪個位置。

```cpp
struct Vertex {
    float position[3];
    float uv[2];
};
```

本階段的每面四個角都使用相同 UV：

| 面上的角 | UV |
|---|---|
| 左上 | `(0, 0)` |
| 右上 | `(1, 0)` |
| 右下 | `(1, 1)` |
| 左下 | `(0, 1)` |

原本只有 8 個共用角點，適合示範頂點顏色。但同一個立方體角點同時屬於三個面，而這三個面需要的 UV 可能不同。一個頂點只能帶一組 UV，因此現在使用每面 4 個頂點：6 × 4 = 24。位置可以重複，UV 可以不同。

仍然是 12 個三角形、36 個索引，模型的幾何形狀沒有變。頂點資料為 24 × 5 × 4 = 480 bytes；索引維持 36 × 2 = 72 bytes。

## 每幀如何畫出貼圖

每幀繼續計算 World × View × Projection 矩陣，並更新深度貼圖尺寸。vertex shader 把 3D 位置轉成螢幕位置，也將 UV 傳到下一階段。GPU 會在三角形內插值 UV，包含透視校正。

繪圖前會呼叫：

```cpp
SDL_GPUTextureSamplerBinding textureBinding{};
textureBinding.texture = texture_;
textureBinding.sampler = sampler_;
SDL_BindGPUFragmentSamplers(pass, 0, &textureBinding, 1);
```

這表示把「貼圖 + 取樣規則」放進 fragment shader 的第 0 個取樣槽。C++ 建立 fragment shader 時也宣告 `num_samplers = 1`。

HLSL 中對應的宣告與取色方式是：

```hlsl
Texture2D<float4> colorTexture : register(t0, space2);
SamplerState colorSampler : register(s0, space2);

return float4(colorTexture.Sample(colorSampler, input.uv).rgb, 1.0f);
```

`t0` 是貼圖，`s0` 是 sampler。`space2` 是 SDL 的 DXIL fragment shader 資源配置規則。`Sample()` 根據目前 UV 找到貼圖中的顏色。

sampler 使用 Linear，會混合附近像素以減少放大時的方塊感；ClampToEdge 讓邊緣取樣停在圖片邊緣。每幀綁定資源不代表重新讀檔或重新上傳圖片。

這一步使用 RGBA8 UNORM，直接呈現貼圖 RGB，沒有光照或色彩運算。之後加入光照時，需一併規劃 sRGB／linear 色彩處理。遠距離縮小時，沒有 mipmaps 的高頻格線可能閃爍；本次保留為已知限制。

## 為什麼要逐列複製

`SDL_Surface::pitch` 是 CPU 圖片每列占用的 bytes，可能包含填補空間。GPU upload 的列距也可能不同。本次將 GPU 列距對齊到 256 bytes，符合 D3D12 的貼圖傳輸對齊需求。

例如 3 像素寬的 RGBA 圖片，每列有效像素是 3 × 4 = 12 bytes，但 upload buffer 每列預留 256 bytes。程式每次只複製 12 bytes，再移到下一列的起點。上傳時透過 `pixels_per_row` 告訴 SDL 這個列距。

載入器也會檢查圖片尺寸、CPU 列距和 GPU transfer buffer 的 32-bit 大小限制，避免資料大小轉型後溢位。

## 誰負責釋放資源

`TextureLoader` 的 CPU surface 使用 `std::unique_ptr` 自動釋放。upload transfer buffer 在提交後交給 SDL 釋放；SDL 會保留 GPU 尚在使用的資源直到安全時機。

成功回傳的 GPU 貼圖由 `CubeRenderer` 持有。關閉時先等待 GPU，再釋放 sampler、貼圖、頂點／索引及 pipeline，最後釋放視窗綁定和 GPU device。

如果 PNG 不存在、內容損壞或 GPU 資源建立失敗，初始化會回報包含檔案路徑的錯誤並結束，不會默默畫出一個沒有貼圖的立方體。

## 本次驗證

- Debug/x64：主程式、GpuTriangle 和四個既有測試目標建置成功。
- CTest：InputActionsTests、WindowSettingsTests、MathTypesTests、GameTests，4/4 通過。
- 主程式 `--smoke-test`：讀入 512×512 PNG，成功繪製 30 幀。
- GpuTriangle `--test-display-settings`：尺寸、全螢幕、換螢幕、最大化與最小化恢復通過。
- 暫存 GPU 檢查：分別建立 3×2 RGB 與 RGBA PNG，透過正式載入器上傳 GPU，再讀回逐像素比較；RGB、Alpha、列距與上下順序全數符合。
- 暫存 GPU 檢查：缺少檔案與無效 PNG 均回傳失敗並顯示明確錯誤。
- 使用正式的立方體頂點、shader 和 pipeline 建立離屏圖像，檢視正面與斜角圖，確認格子座標方向、取樣及深度遮擋。

暫存檢查程式與圖像位於忽略提交的 `build/texture-validation/`，不是新增的 CTest 項目。`cube-front.png` 和 `cube-angled.png` 是 GPU 離屏輸出，不是桌面截圖。

可重跑正式專案檢查：

```powershell
& $cmakeExe --build build --config Debug --target BiggerFISTsSDL GpuTriangle InputActionsTests WindowSettingsTests MathTypesTests GameTests
& (Join-Path (Split-Path -Parent $cmakeExe) 'ctest.exe') --test-dir build -C Debug --output-on-failure
& '.\build\Debug\BiggerFISTsSDL.exe' --smoke-test
& '.\build\Debug\GpuTriangle.exe' --test-display-settings
```

## 目前範圍

完成的是單張 PNG 貼圖、不透明且無光照的立方體。模型仍是 C++ 頂點資料；OBJ／GLB 載入、多材質、透明混合、光照與 Blender 動畫都尚未加入。下一步可以把頂點資料的來源換成一個原版 OBJ，重用本次完成的貼圖上傳與取樣流程。

# BiggerFISTs SDL3：第一個視窗

這是移植的基礎練習。執行後會出現 1280×720 的深藍色視窗與白色方塊。使用 WASD 移動方塊，Space 更改背景顏色，按 `Esc` 或視窗右上角的 `X` 即可關閉。視窗可以調整大小，方塊位置會限制在畫面範圍內。

目前使用 SDL3 的簡易 2D renderer 繪圖；遊戲模型和戰鬥程式尚未移植。`Game` 管理方塊狀態、移動與畫面內容，`FrameTimer` 處理時間差、FPS 記錄與每幀等待。`Input` 查詢鍵盤狀態與按下／放開事件，`InputActions` 將按鍵轉成 `GameInput` 遊戲動作。`Application` 負責 SDL 初始化、視窗、事件、主要迴圈與資源釋放，`main.cpp` 啟動 `Application` 並回傳執行結果。之後的 3D 階段會改用 SDL GPU。

## 專案檔案

- `src/main.cpp`：程式入口，啟動 `Application`。
- `src/Application.h`：執行管理器與 `Run()` 的宣告。
- `src/Application.cpp`：SDL 初始化、視窗、事件、主要迴圈與資源釋放。
- `src/FrameTimer.h`：時間模組的宣告與計時資料。
- `src/FrameTimer.cpp`：實際時間差計算、每秒 FPS 記錄與剩餘時間等待。
- `src/Input.h`：鍵盤輸入模組的宣告、狀態指標與本幀事件紀錄。
- `src/Input.cpp`：查詢按住、剛按下與剛放開，排除按鍵重複事件與無效索引。
- `src/InputActions.h`：鍵盤輸入轉成遊戲動作的介面。
- `src/InputActions.cpp`：將鍵盤輸入轉成移動、改色與退出動作。
- `src/InputBindings.h`：預設按鍵對應與設定讀取函式的宣告。
- `src/InputBindings.cpp`：讀取外部按鍵設定、驗證動作與按鍵名稱。
- `src/GameInput.h`：遊戲動作資料，包含移動方向與動作狀態。
- `src/Game.h`：遊戲類別的宣告與成員資料。
- `src/Game.cpp`：方塊移動、邊界限制、背景顏色與繪圖。
- `src/GameConfig.h`：設定資料結構與讀取函式的宣告。
- `src/GameConfig.cpp`：設定檔讀取與數值驗證的實作。
- `CMakeLists.txt`：告訴 CMake 如何編譯程式並連結 SDL3。
- `config/gameplay.cfg`：外部移動速度、移動時間上限與目標 FPS 設定；建置時會放到執行檔旁邊。
- `config/input.cfg`：外部鍵盤對應；建置時會放到執行檔旁邊。
- `vendor/SDL`：固定在 SDL3 `release-3.4.16` 的官方原始碼。
- `build`：CMake 產生的檔案；可以重新產生，不需要手動編輯。
- `tests/InputActionsTests.cpp`：快速點按、事件重複、索引檢查、替換按鍵對應與設定檔讀取的回歸測試。

這個專案不會讀取或修改原本的 KamataEngine 專案。

## 鍵盤與遊戲動作

每幀依序呼叫 `Input::BeginFrame()`、將 SDL 事件交給 `ProcessEvent()`、呼叫 `Update()`，再透過 `InputActions::Evaluate()` 產生遊戲動作。`Game` 使用 `GameInput` 更新移動與畫面內容。

| 動作 | 預設按鍵 | `InputBindings` 成員 |
|---|---|---|
| 向左移動 | A | `moveLeft` |
| 向右移動 | D | `moveRight` |
| 向上移動 | W | `moveUp` |
| 向下移動 | S | `moveDown` |
| 更改背景顏色 | Space | `changeBackground` |
| 退出 | Esc | `quit` |

按住狀態來自 SDL 管理的陣列，按下與放開由本幀事件記錄。快速點按即使在同一幀內完成，也能同時保留 `pressed` 與 `released`；按住產生的重複事件不會反覆觸發 `pressed`。改色動作按下、放開時，會分別印出 `ChangeBackground pressed` 與 `ChangeBackground released`。

程式啟動時透過 `LoadInputBindings()` 讀取外部按鍵設定，再傳給 `InputActions` 建構子。`InputBindings` 的 C++ 預設值會在檔案遺失或設定無效時使用。

## 外部按鍵設定

`config/input.cfg` 每行格式是「動作名稱 按鍵名稱」：

```text
moveLeft A
moveRight D
moveUp W
moveDown S
changeBackground Space
quit Escape
```

例如將 `changeBackground Space` 改成 `changeBackground Return`，就會使用 Enter 更改背景；將 `moveUp W` 改成 `moveUp Q`，就會使用 Q 向上移動。按鍵名稱使用 SDL scancode 名稱，像 `Left Shift`、`Right Ctrl`、`Up` 都可以使用；含空格的按鍵名稱不需要加引號。

儲存後在 Visual Studio 按 F5，CMake 會更新執行檔旁邊的 `input.cfg`，程式重新啟動時讀取新對應。也可以直接修改 `build/Debug/input.cfg`，重新執行 `.exe` 來改鍵，不需要重新建置；之後 CMake 重新產生檔案時，會以 `config/input.cfg` 為準。

目前每個動作對應一個鍵盤按鍵，設定只在啟動時讀取。動作名稱必須與範例一致；空白行會跳過，未知動作、未知按鍵或缺少按鍵會顯示行號並跳過該行。其他有效行仍會載入。未設定的動作保留預設值，重複設定以最後一個有效值為準；檔案遺失時全部使用預設值。

## 建置與執行

需要安裝 Visual Studio 的 **Desktop development with C++** 工作負載。本機已使用 Visual Studio Community 2026 和 CMake 驗證。

1. 開啟 PowerShell。
2. 輸入以下命令，進入專案資料夾：

   ```powershell
   Set-Location 'G:\class\GameCreator\EX\robot game 3D 001\BiggerFISTsSDL'
   ```

3. 設定本機的 CMake 路徑並產生 Visual Studio 建置檔：

   ```powershell
   $cmakeExe = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
   & $cmakeExe -S . -B build -G 'Visual Studio 18 2026' -A x64
   ```

4. 編譯：

   ```powershell
   & $cmakeExe --build build --config Debug --target BiggerFISTsSDL
   ```

5. 執行：

   ```powershell
   & '.\build\Debug\BiggerFISTsSDL.exe'
   ```

`build\Debug` 裡應有 `BiggerFISTsSDL.exe` 和 `SDL3.dll`。若視窗一閃就關閉，請從 PowerShell 執行第 5 步並查看顯示的錯誤訊息。

若之後從 Git 重新取得此專案，請先在專案根目錄執行 `git submodule update --init --recursive`，取得 `vendor/SDL`。

## 輸入測試

在專案資料夾中，設定上方的 `$cmakeExe` 後執行：

```powershell
& $cmakeExe --build build --config Debug --target InputActionsTests
& (Join-Path (Split-Path -Parent $cmakeExe) 'ctest.exe') --test-dir build -C Debug --output-on-failure
```

測試使用 SDL 的 dummy video driver 與合成鍵盤事件，驗證本幀事件清除、快速點按、排除重複事件、無效按鍵與替換改色按鍵。也會建立並刪除暫存設定檔，驗證各動作改鍵、含空格的名稱、UTF-8 BOM、CRLF、錯誤行、重複設定與檔案遺失。它不會操作使用者的實體鍵盤。

## 外部遊戲設定

`config/gameplay.cfg` 每行接受一個設定：

```text
moveSpeed 240.0
maxMovementDeltaTime 0.05
targetFps 60
```

| 設定 | 用途 | 接受範圍 | 預設值 |
|---|---|---|---|
| `moveSpeed` | 每秒移動的像素數 | 大於 0，最多 2000 | 240.0 |
| `maxMovementDeltaTime` | 單次移動計算使用的秒數上限 | 大於 0，最多 0.1 | 0.05 |
| `targetFps` | 目標每秒畫面數 | 1 到 240 的整數 | 60 |

這些範圍是本專案的設定限制。實際 FPS 仍受電腦效能影響；實際 frame delta 超過 `maxMovementDeltaTime` 時，移動使用的時間會被截短，因此低 FPS 或卡頓時移動可能變慢。

改成 `moveSpeed 480.0` 後，在 Visual Studio 2026 按 F5。CMake 會更新執行檔旁邊的設定檔，程式重新啟動時會讀取新速度，不需要修改 C++。

也可以直接修改 `build/Debug/gameplay.cfg`，重新啟動該資料夾內的 `.exe` 來驗證不重新建置即可改速度；之後 CMake 重新產生檔案時，會以 `config/gameplay.cfg` 為準。程式只在啟動時讀取設定，尚未支援執行中重新載入。

程式會印出載入的三個設定。空白行會跳過；鍵名錯誤、數值超出範圍、非有限數或該行有多餘內容時，會記錄行號並跳過該行，繼續讀取其他設定。未成功讀入的設定保留 `GameConfig` 預設值；重複設定以最後一個有效值為準。檔案遺失時全部使用預設值。視窗尺寸改由下方的 `display.cfg` 設定。

## SDL GPU 三角形與顯示設定

在 Visual Studio 中，對 `GpuTriangle` 按右鍵並設為啟始專案，選擇 Debug／x64 後按 F5。CMake 會編譯 C++ 與兩個 HLSL shader，再將 SDL3.dll 和設定檔放到執行檔旁邊。

白色方塊的 `BiggerFISTsSDL` 也使用相同的 `display.cfg` 與以下快捷鍵。要測試白色方塊，將 `BiggerFISTsSDL` 設為啟始專案，重新建置後按 F5。先點一下遊戲視窗，讓它取得鍵盤焦點，再按快捷鍵。

| 按鍵 | 功能 |
|---|---|
| F11 | 切換視窗／無邊框全螢幕 |
| F1 | 回到視窗模式，設定 1280×720 |
| F2 | 回到視窗模式，設定 1600×900 |
| F8 | 移到下一台螢幕，保留目前的顯示模式 |
| Esc | 結束程式 |

快捷鍵忽略按住時產生的重複事件。只有一台螢幕時，F8 會顯示訊息並保持原位。拖曳改變的視窗大小也會被記住；離開全螢幕時，會恢復視窗大小與位置。視窗尺寸會受指定螢幕的可用範圍限制。若切換螢幕，視窗會在新螢幕置中。

`config/display.cfg` 提供啟動預設值：

```text
windowWidth 1280
windowHeight 720
fullscreen 0
displayIndex 0
```

- `windowWidth`：320 到 7680 的整數，表示視窗內容寬度。
- `windowHeight`：240 到 4320 的整數，表示視窗內容高度。
- `fullscreen`：0 為視窗，1 為無邊框全螢幕。
- `displayIndex`：SDL 目前螢幕清單中的索引；0 是第一台，1 是第二台。索引不存在時改用主螢幕。這個索引不是 Windows 顯示設定中的螢幕編號，也不是可跨次啟動保存的 SDL DisplayID。

儲存來源設定後，重新建置並執行即可套用。也可以直接修改 `build/Debug/display.cfg` 並重啟執行檔。快捷鍵的更改只影響目前執行，不會寫回設定檔。

無邊框全螢幕使用桌面的解析度，`windowWidth` 與 `windowHeight` 保留回到視窗時使用的大小。標題列及日誌會顯示程式名稱、實際模式、輸出像素尺寸與使用的螢幕。內部渲染解析度與獨佔全螢幕尚未加入。

`src/WindowSettings.h/.cpp` 負責設定讀取、快捷鍵要求、套用、幾何恢復與螢幕遺失時的預設選擇。`ProcessEvent()` 收集更改，`ApplyPending()` 在每幀開始繪圖前套用，避免繪圖途中改變視窗。`Application.cpp` 與 `GpuTriangle.cpp` 都接入這個模組。切換時透過 `SDL_SyncWindow()` 等待系統完成；切換可能有短暫等待。套用失敗會嘗試恢復先前設定，並以實際視窗狀態更新紀錄。

`GpuTriangle.cpp` 透過 backbuffer 的實際寬、高更新輸出資訊；最小化時若沒有 backbuffer，會跳過繪製。設定檔錯誤行顯示行號並跳過，其餘行照常讀取；缺少檔案時使用預設值。

`Application.cpp` 透過 `SDL_GetRenderOutputSize()` 更新輸出資訊及方塊移動邊界；改變大小後，方塊仍會限制在目前畫面內。

顯示設定測試：

```powershell
& $cmakeExe --build build --config Debug --target WindowSettingsTests
& (Join-Path (Split-Path -Parent $cmakeExe) 'ctest.exe') --test-dir build -C Debug --output-on-failure
```

CTest 使用 dummy video driver 驗證設定讀取、事件排程、按鍵重複、全螢幕往返、視窗大小恢復及螢幕預設選擇。實體多螢幕與 GPU 繪圖可透過執行 GpuTriangle 並使用上述按鍵檢查。

GPU 繪圖切換自動檢查：

```powershell
& '.\build\Debug\GpuTriangle.exe' --test-display-settings
```

它會在自己的視窗中依序測試大小、全螢幕、下一台螢幕、最大化後選擇視窗大小、最小化與還原，成功繪製後印出 PASS 並結束；不會操作實體鍵盤，也不會修改桌面解析度。多螢幕分支需要電腦實際連接兩台以上螢幕才能驗證。

## Vertex Buffer 三角形

以下保留前一步三角形的教學紀錄。目前程式已進入下一節的 Index Buffer 四邊形階段；頂點資料、建立函式及繪圖呼叫以該節為準。

這一步使用 `GpuTriangle`，請在 Visual Studio 將它設為啟始專案，選擇 Debug／x64 後按 F5。正常畫面仍為上方紅色、右下綠色、左下藍色的漸層三角形。

三角形的位置與顏色現在集中放在 `src/GpuTriangle.cpp` 的 `kTriangleVertices`，Vertex Shader 不再內建三組位置與顏色。

```cpp
struct Vertex {
    float position[3];
    float color[3];
};

constexpr Vertex kTriangleVertices[] = {
    { { 0.0f, 0.6f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
    { { 0.6f, -0.6f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
    { { -0.6f, -0.6f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
};
```

每列代表一個頂點。第一組是位置 x、y、z，第二組是顏色 r、g、b。這裡 z 都為 0，仍是平面三角形，尚未加入世界座標、相機或透視投影。

在目前 Windows 平台，`float` 是 4 bytes；每個頂點有 6 個 `float`，共 24 bytes，三個頂點共 72 bytes。程式用 `sizeof` 計算大小，用 `offsetof` 計算欄位位置，並以 `static_assert` 檢查頂點結構具有預期的記憶體排列。

`CreateTriangleVertexBuffer()` 在啟動時執行一次：

1. `SDL_CreateGPUBuffer()` 建立用途為 VERTEX 的 GPU Buffer。
2. `SDL_CreateGPUTransferBuffer()` 建立用途為 UPLOAD 的暫存傳輸區。
3. `SDL_MapGPUTransferBuffer()` 取得 CPU 可寫入的位址。
4. `std::memcpy()` 將 C++ 頂點陣列複製進傳輸區。
5. `SDL_UnmapGPUTransferBuffer()` 結束 CPU 寫入。
6. 取得 Command Buffer，開始 Copy Pass，透過 `SDL_UploadToGPUBuffer()` 記錄上傳操作。
7. 結束 Copy Pass 並提交 Command Buffer，讓 GPU 執行上傳。
8. 釋放 Transfer Buffer 的使用權；SDL 會等到安全時機才真正回收它。

提交成功不代表 GPU 已立即做完；GPU 會按照提交順序處理後續工作，因此接著提交的繪圖可以使用這份資料。此處不需要在每幀等待 GPU 或重新上傳靜態頂點。

`CreatePipeline()` 用 Vertex Input State 告訴 GPU 如何讀取資料：

| 資料 | Buffer Slot | Location | Format | Offset |
|---|---|---|---|---|
| position | 0 | 0 | FLOAT3 | `offsetof(Vertex, position)`，目前為 0 |
| color | 0 | 1 | FLOAT3 | `offsetof(Vertex, color)`，目前為 12 |

`pitch = sizeof(Vertex)` 表示讀下一個頂點時，往後移動 24 bytes。Location 對應 Vertex Shader 的輸入；SDL 的預設 Direct3D 12 路徑使用 `TEXCOORD0`、`TEXCOORD1` 表示這兩個輸入，即使資料是位置或顏色，也使用這個命名規則。

`shaders/triangle.vert.hlsl` 接收位置及顏色，將位置補成 `float4(input.position, 1.0f)`，顏色繼續傳給 Fragment Shader。Fragment Shader 保留原本的顏色輸出；GPU 在三角形內插值，所以會形成漸層。

每幀 `RunLoop()` 先綁定 Pipeline，再用 `SDL_BindGPUVertexBuffers()` 綁定 Slot 0 的頂點資料，最後呼叫 `SDL_DrawGPUPrimitives()`。頂點數由陣列大小計算，這裡是 3；TRIANGLELIST 將每三個頂點組成一個三角形。

結束程式時釋放 Vertex Buffer 與 Pipeline。建立、映射及提交失敗時會記錄錯誤並清理已建立的資源，不會帶著無效的 Buffer 進入繪圖迴圈。

可以把第一個頂點的 y 從 `0.6f` 改成 `0.3f`，儲存後重新建置 `GpuTriangle` 並執行，三角形頂端就會下降。只改 C++ 頂點資料不需要修改 shader；目前資料仍在 C++ 中，尚未加入模型檔案讀取或執行中更新頂點。

## Index Buffer 四邊形

以下記錄前一步的靜態四邊形。目前程式已加入下一節的 Transform Matrix，執行後會顯示保持比例的旋轉四邊形。

目前仍執行 `GpuTriangle`，但視窗標題改為 `SDL GPU Indexed Quad`。在 Visual Studio 將 `GpuTriangle` 設為啟始專案，選擇 Debug／x64 後重新建置並按 F5，即可看到左上紅、右上綠、右下藍、左下黃的彩色四邊形。

`src/GpuTriangle.cpp` 現在使用以下資料：

```cpp
constexpr Vertex kQuadVertices[] = {
    { { -0.6f, 0.6f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
    { { 0.6f, 0.6f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
    { { 0.6f, -0.6f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
    { { -0.6f, -0.6f, 0.0f }, { 1.0f, 1.0f, 0.0f } }
};

constexpr Uint16 kQuadIndices[] = {
    0, 1, 2,
    0, 2, 3
};
```

頂點編號就是陣列下標，從 0 開始。Vertex Buffer 儲存位置和顏色；Index Buffer 儲存頂點編號。第一個三角形引用 0、1、2，第二個引用 0、2、3，因此兩個三角形共用頂點 0 與 2。

```text
0 ------- 1
| \       |
|   \     |
|     \   |
3 ------- 2
```

四個頂點各 24 bytes，共 96 bytes；六個 `Uint16` 索引各 2 bytes，共 12 bytes。頂點數、索引數和資料大小都從陣列計算。若將相同屬性的共用頂點展開成六個獨立頂點，頂點資料會占用 144 bytes；這裡使用 96 + 12 = 108 bytes。位置相同但顏色、UV 或法線不同的頂點，仍可能需要分開存放。

`AreQuadIndicesValid()` 在編譯時檢查所有索引均小於頂點數。`static_assert()` 也確認索引數是 3 的倍數，以及 `Uint16` 為 2 bytes；無效的固定資料會讓編譯停止，而不是等到 GPU 讀取越界。

`CreateUploadedBuffer(device, data, dataSize, usage)` 是從前一步的上傳函式調整而來，讓 Vertex Buffer 與 Index Buffer 共用同一套建立、映射、複製、提交及失敗清理流程：

| 參數 | 用途 |
|---|---|
| `device` | GPU Device |
| `data` | 要複製的資料起始位址 |
| `dataSize` | 要複製的 bytes 數 |
| `usage` | GPU Buffer 用途；這裡分別傳 VERTEX 或 INDEX |

`CreateQuadVertexBuffer()` 上傳 `kQuadVertices`，用途為 `SDL_GPU_BUFFERUSAGE_VERTEX`。`CreateQuadIndexBuffer()` 上傳 `kQuadIndices`，用途為 `SDL_GPU_BUFFERUSAGE_INDEX`。兩者是各自建立的 GPU Buffer，啟動時分別上傳一次，之後繪圖重複使用。

每幀先綁定 Pipeline 與 Vertex Buffer，再執行：

```cpp
SDL_GPUBufferBinding indexBinding{};
indexBinding.buffer = indexBuffer;
indexBinding.offset = 0;
SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
SDL_DrawGPUIndexedPrimitives(pass, kQuadIndexCount, 1, 0, 0, 0);
```

`indexBinding.offset` 是 Buffer 中的 byte offset；這裡為 0，從資料開頭讀取。`SDL_GPU_INDEXELEMENTSIZE_16BIT` 告訴 GPU 每個索引是 2 bytes，必須與 C++ 的 `Uint16` 一致。

索引繪圖的六個參數依序是 Render Pass、索引數、instance 數、起始索引、加到每個頂點編號的偏移，以及起始 instance。這裡使用六個索引、繪製一個 instance，其餘偏移全部為 0。傳入的繪圖數量是索引數 6，並非頂點數 4。

Shader 與 Vertex Input State 保持相同；索引先決定要讀取哪個頂點，Vertex Shader 仍接收該頂點的位置與顏色。TRIANGLELIST 每三個索引組成一個三角形，所以六個索引形成兩個三角形。

結束時先釋放 Index Buffer，再釋放 Vertex Buffer 與 Pipeline。Index Buffer 建立失敗時，也會釋放已建立的 Vertex Buffer，不會進入繪圖迴圈。

可以將頂點 2 的 x 從 `0.6f` 改成 `0.3f`，重新建置並執行。因為兩個三角形都引用頂點 2，右下角及兩個三角形會一起改變。這一步仍使用直接輸出的平面座標；尚未加入相機、透視投影、深度測試或模型載入。

## Transform Matrix 與 Vertex Uniform

本節記錄前一步的四邊形實作；目前執行結果已更新為下一節的 3D 立方體。

目前執行目標仍為 `GpuTriangle`，視窗標題是 `SDL GPU Transform Quad`。在 Visual Studio 將 `GpuTriangle` 設為啟始專案，選擇 Debug／x64 後重新建置並按 F5。四邊形預設縮放為 0.65 倍，每秒逆時針旋轉 45 度，8 秒完成一圈。

`src/MathTypes.h/.cpp` 提供這一步所需的矩陣計算，不依賴 SDL 或其他數學函式庫：

| 函式 | 用途 |
|---|---|
| `MakeIdentityMatrix()` | 維持原本的位置 |
| `MakeTranslationMatrix(x, y, z)` | 位移 |
| `MakeRotationZMatrix(radians)` | 繞 Z 軸旋轉，角度使用 radians |
| `MakeScaleMatrix(x, y, z)` | 各軸縮放 |
| `MultiplyMatrices(left, right)` | 合併兩個矩陣 |
| `MakeAspectCorrectionMatrix(aspectRatio)` | 使用輸出比例校正圖形，無效比例回到 Identity |

```cpp
struct alignas(16) Matrix4x4 {
    float elements[4][4]{};
};
```

矩陣有 4 列、4 欄，共 16 個 `float`，大小為 64 bytes。資料依列儲存，使用 row vector：`position * matrix`。`alignas(16)` 指定 16-byte 對齊；`static_assert` 確認大小與對齊符合預期。

Identity Matrix 的對角線是 1，其餘為 0。Scale Matrix 將對角線的 x、y、z 改成縮放量；Translation Matrix 將位移放在最後一列的前三欄。這個放置方式與 row vector 規則一致，因此位置乘上矩陣後會加上位移。

`src/GpuTriangle.cpp` 的 `QuadTransformSettings` 集中放置調整值：

```cpp
struct QuadTransformSettings {
    float positionX = 0.0f;
    float positionY = 0.0f;
    float scale = 0.65f;
    float initialAngleDegrees = 0.0f;
    float rotationSpeedDegreesPerSecond = 45.0f;
};
```

`positionX`、`positionY` 是校正前的圖形座標位移，不是像素。正 x 向右、正 y 向上。`initialAngleDegrees` 是起始角度；旋轉速度為正時逆時針，負時順時針，0 時停止自動旋轉。修改這些 C++ 設定後，需要重新建置再執行。

`RunLoop()` 記錄動畫開始的 `SDL_GetTicksNS()`，每幀用現在時間減去開始時間並除以 1000000000，取得經過的秒數。`BuildQuadTransform()` 計算：

```text
angleDegrees = initialAngleDegrees + elapsedSeconds * rotationSpeedDegreesPerSecond
```

這裡使用真實時間，不依賴累計畫面數。暫停或最小化後恢復時，角度也會反映已經經過的時間。角度先以 `std::fmod(..., 360.0)` 限制在一圈內，再從 degrees 換成 radians，避免長時間運行後將很大的角度直接轉成 `float`。

矩陣的合併順序是：

```text
transform = Scale * RotationZ * Translation * AspectCorrection
positionOut = positionIn * transform
```

因為頂點放在左側，依序是先縮放、再旋轉、再位移、最後校正畫面比例。順序會影響結果，例如先位移再旋轉，位移也會跟著旋轉，可能變成繞其他位置轉動。

比例取自 backbuffer 的實際寬高，`aspectRatio = outputWidth / outputHeight`。橫向畫面將 x 乘上 `1 / aspectRatio`；直向畫面將 y 乘上 `aspectRatio`。這讓 x、y 使用相同的像素比例，並以畫面的短邊作為大小基準。視窗改變大小或切換螢幕後，每幀會重新計算；不需要改頂點資料。輸出寬高為 0 時，建構函式回到 Identity，避免除以 0。

Vertex Shader 新增一個 Uniform：

```hlsl
cbuffer TransformUniforms : register(b0, space1) {
    row_major float4x4 transform;
};
```

`b0, space1` 對應 SDL Direct3D 12 的 Vertex Uniform Slot 0。`row_major` 明確表示依列儲存，與 C++ 的排列一致。載入 Vertex Shader 時設定 `num_uniform_buffers = 1`；Fragment Shader 沒有使用 Uniform，仍為 0。

每幀在繪圖前呼叫：

```cpp
SDL_PushGPUVertexUniformData(commands, 0, &transform, static_cast<Uint32>(sizeof(transform)));
```

它把這次繪圖要使用的 64-byte 矩陣資料交給 SDL 管理。Vertex Buffer 與 Index Buffer 仍然在啟動時上傳一次；動畫期間只更新 Uniform，不重新上傳四個頂點。

Shader 中的位置信息改為：

```hlsl
output.position = mul(float4(input.position, 1.0f), transform);
```

`mul` 表示矩陣乘法。`float4` 把 x、y、z 補上 w = 1，讓位移也能參與計算；顏色仍沿用原本資料與插值。這一步的矩陣是平面變換和比例校正，尚未加入 View Matrix、透視投影或 Depth Buffer。

可以把 `rotationSpeedDegreesPerSecond` 改成 0，讓圖形停住，再分別調整位置、起始角度與縮放來觀察效果。每次先改一個數值，容易分辨是哪種變換造成畫面改變。

數學測試：

```powershell
& $cmakeExe --build build --config Debug --target MathTypesTests
& (Join-Path (Split-Path -Parent $cmakeExe) 'ctest.exe') --test-dir build -C Debug --output-on-failure
```

`MathTypesTests` 驗證 Identity、位移、縮放、正負 90 度旋轉、矩陣組合順序，以及橫向、直向和無效比例處理。GPU 顯示切換檢查仍可執行 `GpuTriangle.exe --test-display-settings`。

## 3D Cube、Camera、Perspective 與 Depth Buffer

目前執行目標為 `GpuTriangle`，視窗標題是 `SDL GPU Perspective Cube`。在 Visual Studio 將 `GpuTriangle` 設為啟始專案，選擇 Debug／x64，重新建置並按 F5。`BiggerFISTsSDL` 是先前的白色方塊程式。

立方體使用 8 個頂點與 36 個 `Uint16` 索引：6 個面，每面 2 個三角形，每個三角形使用 3 個索引。Vertex Buffer 為 192 bytes，Index Buffer 為 72 bytes；啟動時上傳一次。頂點顏色會在三角形內插值，目前沒有光照或材質貼圖。

`src/GpuTriangle.cpp` 的 `CubeSceneSettings` 集中設定物件與相機：

| 設定 | 預設值 | 用途 |
|---|---|---|
| `position` | `(0, 0, 0)` | 立方體在世界中的位置 |
| `scale` | `1` | 立方體縮放 |
| `initialXDegrees` / `initialYDegrees` | `20` / `30` | 起始旋轉角度 |
| `rotationXDegreesPerSecond` / `rotationYDegreesPerSecond` | `25` / `40` | 每秒旋轉角度 |
| `cameraPosition` | `(0, 0, -3)` | 相機位置 |
| `cameraTarget` | `(0, 0, 0)` | 相機看的位置 |
| `cameraUp` | `(0, 1, 0)` | 相機向上的參考方向 |
| `verticalFovDegrees` | `60` | 垂直視角 |
| `nearPlane` / `farPlane` | `0.1` / `100` | 相機可見深度範圍 |

設定目前是 C++ 常數，修改後需要重新建置。旋轉使用實際經過秒數計算，不依賴幀數；最小化或暫停後會反映已經經過的時間。

### World、View、Projection

`MathTypes` 新增 `Vector3`、向量相減、Dot、Cross、Normalize、X／Y 軸旋轉、LookAt 與 Perspective 計算，不新增外部數學函式庫。

```text
World = Scale * RotationX * RotationY * Translation
transform = World * View * Projection
clipPosition = localPosition * transform
```

World 將模型座標移到世界中；View 將世界座標轉成相機座標；Projection 產生透視投影需要的 clip position。沿用 row vector 與 row-major，依序從左往右作用。每幀傳給 Vertex Shader 的資料仍是一個 64-byte 矩陣。

`TryMakeLookAtLHMatrix()` 用相機位置、目標與向上方向建立 View。相機座標中正 Z 是前方；目前相機從世界 `(0, 0, -3)` 看向原點。相機與目標不能重合，向上方向也不能與視線平行。無效設定會回報失敗。

`TryMakePerspectiveLHMatrix()` 使用垂直 FOV、實際 GPU 輸出寬高比與 Near／Far。GPU 會用 clip position 的 w 進行透視除法，因此距離更遠的同尺寸物件看起來更小。Near 映射到深度 0，Far 映射到 1；中間深度不是線性距離。必須滿足 `0 < FOV < 180`、`aspectRatio > 0`、`0 < nearPlane < farPlane`。

畫面比例現在由 Projection 處理，沒有再乘上先前的 `AspectCorrection`。固定垂直 FOV 時，改變寬高會改變可見範圍；立方體不會因此被拉長。Pipeline 明確啟用 `enable_depth_clip`，讓近／遠裁切生效。

### Depth Buffer

`DepthBuffer` 管理一張與 GPU 輸出尺寸相同的深度貼圖。每幀清除成 1，Pipeline 使用 `LESS` 比較並寫入深度：新的片段深度比該位置記錄的深度小，才可覆蓋顏色與深度。這讓近處的面遮住遠處的面，不需要靠三角形繪製順序決定遮擋。

優先選擇 D32_FLOAT，若不支援則嘗試 D24_UNORM、D16_UNORM。D3D12 的 optimized clear depth 設定成 1，與 Render Pass 的清除值一致。本階段不使用 stencil。

每次取得 backbuffer 後，使用其實際寬高更新深度貼圖。同尺寸直接重用；尺寸改變時先建立新貼圖，成功後才釋放舊貼圖。建立失敗會保留舊資源並結束這次執行；0 尺寸也會回報失敗。SDL 會等 GPU 安全時才真正回收已釋放的貼圖。

`DepthBuffer` 是 `RunLoop()` 的區域物件，離開迴圈時由 destructor 釋放貼圖，時間點在 GPU Device 銷毀之前。

### 驗證與練習

`MathTypesTests` 新增 X／Y 旋轉、Dot／Cross／Normalize、固定及側面相機、無效相機設定、Near／Far 深度、透視大小，以及無效投影參數測試。既有 InputActions、WindowSettings 測試仍保留。

GPU 驗證涵蓋前面遮住後面、反轉三角形繪製順序後結果一致、遠處物件較小、橫向與直向畫面比例、近／遠與相機後方裁切，以及深度貼圖尺寸更新。

可以每次改一組數值再重新建置：

1. 將兩個旋轉速度改成 0，觀察固定立方體。
2. 將 `cameraPosition.z` 改成 `-5.0f`，相機遠離立方體，物件看起來會更小。
3. 將 `verticalFovDegrees` 改成 `90.0f`，視角變寬，物件看起來會更小。
4. 將 `position.z` 改成 `2.0f`，立方體遠離目前的相機。

目前相機固定，尚未加入相機移動輸入、模型載入或光照。F11、F1、F2、F8 沿用顯示設定操作。

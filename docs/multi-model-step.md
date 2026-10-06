# 同一畫面顯示多個模型

`MultiModelPreview` 是獨立的多模型顯示範例。預設 `config/scene.cfg` 讓同一個視窗左側顯示 Target，右側顯示原版戰鬥場景使用的 LfistTEST；兩者都使用各自的 PNG，並以不同方向旋轉。主程式 `BiggerFISTsSDL` 預設仍依 `config/model.cfg` 一次選擇一個模型；加入 `--scene-preview` 時，也會讀取同一份 `scene.cfg`。

## 如何在 Visual Studio 執行

1. 開啟 `build/BiggerFISTsSDL.slnx`。如果 Visual Studio 已開著舊解決方案，先重新載入，讓它看到新的 `MultiModelPreview` 專案。
2. 在「方案總管」對 `MultiModelPreview` 按右鍵，選「設為啟始專案」。
3. 上方選擇 `Debug`、`x64`，按 `F5`。第一次會先編譯程式並將模型、貼圖和 shader 複製到 `build/Debug/`。
4. 視窗可以拖曳改大小；`F1`／`F2` 切換預設大小，`F11` 切換全螢幕，`F8` 移到下一個螢幕，`Esc` 離開。這個範例沒有 WASD 操作；WASD 仍屬於單模型主程式。

也可以在專案根目錄用 PowerShell 執行：

```powershell
$cmakeExe = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmakeExe -S . -B build -G 'Visual Studio 18 2026' -A x64
& $cmakeExe --build build --config Debug --target MultiModelPreview
& '.\build\Debug\MultiModelPreview.exe'
```

最後一行不加參數會持續顯示畫面。若改成 `& '.\build\Debug\MultiModelPreview.exe' --smoke-test`，繪製 30 幀後會自動結束，適合快速檢查程式有沒有成功啟動。

## 修改場景，不改 C++ 程式

編輯專案的 `config/scene.cfg`。預設內容如下：

```text
camera 0 0 -6

model Target/Target.obj
id target
position -1.6 0 0
scale 1
rotation 0 0
spin 0 20

model LfistTEST/LfistTEST.obj
id left_fist
position 1.6 0 0
scale 1
rotation 15 30
spin 0 -25
```

`camera` 後面依序是 X、Y、Z，這個範例的相機看向原點。每個 `model` 區塊都需要五行設定：`id` 是場景中唯一的物件名稱，`position` 是世界位置 X／Y／Z，`scale` 是大於零的等比例縮放，`rotation` 是起始 X／Y 角度，`spin` 是每秒 X／Y 旋轉角度。負的 `spin` 表示反方向。`model` 路徑從執行檔旁的 `assets/models/` 起算；要再顯示一個 Target，可以複製整個 Target 區塊，同時修改 `id` 與 `position`。同一份 OBJ 出現多次時只載入並上傳一次 mesh／PNG，每個 instance 仍有自己的名稱、位置與旋轉。`id` 規則與用途見 [場景物件 ID 教學](scene-object-id-step.md)。

修改後請重新**建置** `MultiModelPreview`，讓 CMake 將 `config/scene.cfg` 同步到 `build/Debug/scene.cfg`，再按 F5。這裡的建置只是同步設定檔；若 C++ 原始碼沒有變，不需要重編譯 C++。執行中不會自動讀取修改，必須關閉並重新啟動。請修改專案內的設定檔；直接改 `build/Debug/scene.cfg` 僅適合臨時測試，下一次 CMake 設定時可能被覆蓋。

檔案不存在、缺少 `camera` 或模型、模型區塊不完整、重複 `id` 或欄位、無效數字、非正數縮放，以及絕對路徑或含 `..` 的模型路徑，都會在啟動時顯示錯誤並結束。錯誤通常包含設定檔與行號，方便回到原檔修改。`#` 開頭的整行是註解；目前不支援寫在數值後面的行尾註解。

## 程式如何把兩個模型放在同一畫面

1. `SceneConfig.cpp` 讀取執行檔旁的 `scene.cfg`，檢查格式與數值。`SceneLoader.cpp` 依設定從執行檔旁的 `assets/models/` 讀取 OBJ；主程式與獨立範例共用這套載入流程。這些是 SDL 專案中的副本，再由 CMake 複製到 `build/Debug/`；執行時不讀原版 KamataEngine 資料夾。
2. `ObjLoader` 讀 OBJ 的頂點／面、MTL 指定的 PNG，得到兩份 `MeshData`。這裡的 mesh 是「模型資源」，不是畫面中的位置。
3. `ModelScene` 為每個設定檔的 `model` 區塊建立一個 `ModelInstance`。`id` 辨認這個場景物件；`meshIndex` 選擇一份 mesh；`position` 決定放在哪裡，`modelCenter` 由 OBJ 頂點範圍計算，用於預覽旋轉中心。預設值把 Target 放在 X = -1.6，把 LfistTEST 放在 X = 1.6，相機在 Z = -6。
4. `MeshRenderer::Initialize()` 把兩份 mesh 的頂點、索引與 PNG 各自上傳到 GPU。兩個物件共用同一個 GPU device、shader pipeline、sampler 和 depth buffer。
5. 每幀 `MeshRenderer::Render()` 開啟一次 render pass，依序計算各 instance 的 world-view-projection 矩陣、指定它的 GPU mesh 與貼圖，再各執行一次 draw。共用 depth buffer 讓前後遮擋依深度判斷。由於每個 instance 有自己的 transform，兩個模型可以位於不同位置並以不同速度旋轉。

簡單區分：`MeshData` 是「長甚麼樣」，`ModelInstance` 是「使用哪份 mesh、擺在哪裡」，`ModelScene` 是「這一幕有哪些 instance」，`MeshRenderer` 是「把它們畫出來」。同一份 mesh 也可以被多個 instance 使用；這次為了確認兩張 PNG，各放了一種不同模型。

## 已驗證與目前範圍

- `MultiModelPreview --smoke-test` 成功繪製 30 幀，log 顯示 Target 為 1,984 個頂點／960 個三角形，LfistTEST 為 14,132 個頂點／5,192 個三角形，兩張 PNG 各自上傳。
- 離屏 GPU 畫面 `build/texture-validation/multi-model.png` 已人工檢視：Target 在左、拳臂在右，兩者貼圖正確且未被裁切。
- `GameTests` 檢查兩個 instance 的獨立 transform；`SceneConfigTests` 檢查設定格式；`SceneLoaderTests` 檢查共用載入與 mesh 重用。整套 CTest 8 項通過。主程式原模式、主程式場景預覽及獨立範例的 30 幀 smoke test 都通過。
- 實際暫時在 `build/Debug/scene.cfg` 加入第三個 Target，未重編譯 C++ 就成功繪製 3 個 instance；GPU 資源仍只有 2 份。測試後已恢復預設檔案。

這是多模型**渲染預覽**，場景資料存於 `config/scene.cfg`，主程式透過 `--scene-preview` 讀取。尚未定義場景中的玩家或接入戰鬥規則，也沒有加入燈光、骨骼動畫或物件階層。原版 `Lfist.obj` 副本及 `config/model.cfg` 保持原樣；原版 KamataEngine 專案也未修改。主程式執行方式見 [主程式場景預覽教學](main-scene-preview-step.md)。

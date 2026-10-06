# 同一畫面顯示多個模型

這一步新增獨立的 `MultiModelPreview` 執行目標。啟動後，同一個視窗左側是 Target，右側是原版戰鬥場景使用的 LfistTEST；兩者都使用各自的 PNG，並以不同方向旋轉。原本的 `BiggerFISTsSDL` 仍依 `config/model.cfg` 一次選擇一個模型，因此不需要改動目前的模型選擇。

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

## 程式如何把兩個模型放在同一畫面

1. `MultiModelPreview.cpp` 從**執行檔旁邊**的 `assets/models/Target/Target.obj` 與 `assets/models/LfistTEST/LfistTEST.obj` 讀取資料。這些是 SDL 專案中的副本，再由 CMake 複製到 `build/Debug/`；執行時不讀原版 KamataEngine 資料夾。
2. `ObjLoader` 讀 OBJ 的頂點／面、MTL 指定的 PNG，得到兩份 `MeshData`。這裡的 mesh 是「模型資源」，不是畫面中的位置。
3. `ModelScene` 保存兩個 `ModelInstance`。每個 instance 的 `meshIndex` 選擇一份 mesh；`position` 決定放在哪裡，`modelCenter` 決定預覽時繞哪個中心旋轉。本範例把 Target 放在 X = -1.6，把 LfistTEST 放在 X = 1.6，固定相機在 Z = -6。
4. `MeshRenderer::Initialize()` 把兩份 mesh 的頂點、索引與 PNG 各自上傳到 GPU。兩個物件共用同一個 GPU device、shader pipeline、sampler 和 depth buffer。
5. 每幀 `MeshRenderer::Render()` 開啟一次 render pass，依序計算各 instance 的 world-view-projection 矩陣、指定它的 GPU mesh 與貼圖，再各執行一次 draw。共用 depth buffer 讓前後遮擋依深度判斷。由於每個 instance 有自己的 transform，兩個模型可以位於不同位置並以不同速度旋轉。

簡單區分：`MeshData` 是「長甚麼樣」，`ModelInstance` 是「使用哪份 mesh、擺在哪裡」，`ModelScene` 是「這一幕有哪些 instance」，`MeshRenderer` 是「把它們畫出來」。同一份 mesh 也可以被多個 instance 使用；這次為了確認兩張 PNG，各放了一種不同模型。

## 已驗證與目前範圍

- `MultiModelPreview --smoke-test` 成功繪製 30 幀，log 顯示 Target 為 1,984 個頂點／960 個三角形，LfistTEST 為 14,132 個頂點／5,192 個三角形，兩張 PNG 各自上傳。
- 離屏 GPU 畫面 `build/texture-validation/multi-model.png` 已人工檢視：Target 在左、拳臂在右，兩者貼圖正確且未被裁切。
- `GameTests` 增加兩個 instance 的獨立 transform 檢查；整套 CTest 6 項通過。原主程式與立方體範例另外進行回歸執行。

這是多模型**渲染範例**，目前模型清單、位置與旋轉速度寫在 `MultiModelPreview.cpp` 中，尚未移成場景設定檔。沒有加入燈光、骨骼動畫、戰鬥或物件階層。原版 `Lfist.obj` 副本及 `config/model.cfg` 保持原樣；原版 KamataEngine 專案也未修改。

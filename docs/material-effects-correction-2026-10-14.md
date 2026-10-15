# 材質與特效繪製修正紀錄（2026-10-14）

## 基準與範圍

- 原版 `KamataEngine20260622/DirectXGame` 僅作唯讀對照；SDL 版從 `v0.35`、commit `1a28a7d01bb83dc4793451d19bbe82a2b9ec6579` 加上既有未提交變更繼續。
- 修改前的 `git diff --binary` 與 `git status --short` 存在忽略版控的 `build/material-parity-preexisting.patch`、`build/material-parity-preexisting-status.txt`。修改前 Debug CTest 27／27 通過。
- 本次只修正[先前的外觀差異報告](material-effects-appearance-audit-2026-10-14.md)中已能從程式確認的繪製路徑。沒有修改玩法、碰撞、傷害、波次、原版素材或原版程式。

## 已實作

| 項目 | 原版依據 | SDL 修正 |
| --- | --- | --- |
| 模型光照 | 發行版 `Resources/EngineShaders/ObjVS.hlsl`、`ObjPS.hlsl` 使用世界法線、材質 `Ka/Kd/Ks`、wrap diffuse 與 Blinn–Phong specular；引擎 `Material.h` 預設 `Ka=0.3`、`Kd=0.7`、`Ks=0.5`、`wrap=0.5`。隨專案附帶的 Release library 中，`LightGroup::DefaultLightSetting` 把環境光設成白色，啟用三盞白色方向光，方向為 `(0,-1,0)`、`(0.5,0.1,0.2)`、`(-0.5,0.1,-0.2)`；`DirectionalLight::SetDirection` 會正規化方向。原版遊戲程式沒有呼叫自訂 `SetLightGroup`。 | `MeshRenderer` 將已載入的 OBJ 法線作為第三個頂點屬性；頂點 shader 傳世界座標與以 world inverse transpose 變換的法線。像素 shader 依原版公式計算三盞光源，保持既有 alpha discard。缺少 Kd／Ks 的 MTL 使用引擎的材質預設值。 |
| 法線矩陣 | 原版 `ObjVS.hlsl` 使用 world inverse transpose。 | `TryMakeNormalMatrix` 處理非等比縮放、旋轉、鏡射；退化矩陣採 identity 備援，避免零縮放的過場物件讓整幀失敗。加入數值測試。 |
| 火焰層 | 原版 `main.cpp` 的一般 `Object3d` pass 繪製 Rocket／Ultimate／導彈／Charge 火焰；加法 pass 只處理護盾與爆炸。 | 五條火焰建立路徑改用 `World` 與一般深度寫入；護盾和爆炸仍用 `AdditiveEffect`。 |
| 背面剔除 | 原版 `Object3d::PreDraw` 預設 back culling；透明背景也指定 back。加法特效與第一人稱 pass 指定 none。 | 一般世界與透明背景 pipeline 使用 back culling；加法特效、第一人稱維持 none。SDL OBJ 載入時沿 X 鏡射並反轉三角形索引，pipeline 明確指定 clockwise front face。 |

## 驗證

- Debug、Release 全目標建置成功；各 27／27 CTest 通過。`MathTypesTests` 覆蓋非等比縮放、旋轉、鏡射及退化矩陣；`ObjLoaderTests` 覆蓋原版 MTL 省略 Kd 時的材質預設。
- Debug、Release 各九種 30-frame GPU smoke test 全部通過：完整遊戲、兩種第一人稱預覽、六種戰鬥預覽。既有 Day1／Day2／Boss 輸入驅動通關測試屬於通過的 CTest。
- 1280×720 的 [Boss 預覽](../build/material-parity-cull-boss.bmp)、[Rocket tick 42](../build/material-parity-rocket-42.bmp)、[教學／練習場 tick 750](../build/material-parity-shield-750.bmp)，以及 [1600×900 Boss 預覽](../build/material-parity-boss-1600.bmp) 均已實際擷取。1600×900 背景填滿畫面，模型沒有因背面剔除整體消失。暫時修改的 `build/Debug/display.cfg` 已還原原始位元組。
- 1280×720 Title 擷取與修改前的 SDL Title BMP **SHA-256 完全相同**，確認此次 3D shader 改動未影響該 2D 畫面。

## 尚未完成的外觀驗收

- 加入原版程式的光照公式後，SDL 單邊畫面中的天空、城市和部分 Boss 面比舊 SDL 畫面明亮，有區域接近白色。這是可見變化；**沒有同狀態原版戰鬥畫面，不能判定是否過亮，也不以任意減光取代原版數值**。
- 尚無目前原版執行檔在戰鬥、三色護盾、爆炸三段、火焰、晶體及 Boss 死亡的同相機錄影。原版視窗在本工具環境沒有取得鍵盤焦點；本次沒有完成並排短片或實機聽覺比對。
- 晶體受背景影響的透明度、sampler、色彩空間、煙霧亮度等仍列為「缺少可比畫面」。取得原版相同狀態錄影後才做逐項調整。使用者指定的開機／停機黑幕透明度與護盾受擊後顏色規則維持原樣。

## 2026-08-27 舊版原版影片：色彩參考

使用者提供 `C:/Users/kuanl/Videos/Captures/GC2A_07_ヨ_カンリン_BiggerFISTs v0.8 2026-08-27 18-13-39.mp4`。錄影為 1920×1012、55 FPS、約 9 分 29 秒，早於目前 2026-09-26 原版執行檔；HUD、流程、鏡頭和演出不能當作現版本驗收標準。以 FFmpeg 擷取的原尺寸參考畫面保存在忽略版控的 `build/video-reference-20260827/`，未修改原影片。

| 影片位置 | 舊版可觀察的色彩 | 與目前 SDL 擷取畫面的關係 |
| --- | --- | --- |
| 約 100 秒，`frame-100.png` | 天空是中等深度的藍色，山是多層粉紅／深紅，城市是灰色，地板是深灰石材。 | 新增光照後的 SDL `material-parity-cull-boss.bmp` 中，天空偏青白、部分城市表面接近白色；舊 SDL `visual-parity-sdl-boss120.bmp` 的整體色調反而較接近影片。相機和狀態不同，這是**外觀警訊**，不是像素差。 |
| 約 140 秒，`frame-140.png` | Rocket 火焰有明亮的黃心與橘紅外緣；護盾呈青藍色六角格。 | 可用來核對顏色家族及層次；尚無同相機、同事件時間的 SDL 畫面，不判定尺寸與亮度一致。 |
| 約 430、510 秒，`frame-430.png`、`frame-510.png` | Boss 核心偏黑、部件偏白；晶體仍可辨紅／粉、綠等半透明色。 | 目前 SDL Boss 預覽中的遠處晶體與城市較容易被亮光洗淡；視角和距離不同，尚不能定位至晶體 alpha、光照或混色。 |

原版 `ObjPS.hlsl` 在 2026-08-27 到 2026-09-26 之間僅增加透明像素 `clip`，模型光照公式沒有改動；這縮小了可能原因，但不能證明兩次建置的 GPU 狀態、素材與輸出色彩完全相同。下一次調整亮度前，須先核對原版**目前執行檔**的同狀態畫面與材質／光照載入；不要只依舊版影片降低全域光照。舊版 HUD 不列入這份材質驗收。

## 同日追加：材質色彩空間與過亮修正

原版目前執行檔的模型載入以獨立的唯讀診斷程式核對；診斷原始碼和輸出位於忽略版控的 `build/original-material-probe/`，未改原版。SkyDome、floor、BGfront、BossCore、crystleCube 的實際 `Ka=(1,1,1)`、`Kd=(0.7,0.7,0.7)`、`Ks=(0.5,0.5,0.5)`、`alpha=1`、`wrap=0.5`，和 SDL 載入值相符。五個貼圖的 DirectX 格式均為 91，即 `DXGI_FORMAT_B8G8R8A8_UNORM_SRGB`。原版的 `DirectXCommon::PreDraw` 設定 sRGB render target；SDL 之前將模型 PNG 當成 `R8G8B8A8_UNORM`，且在 UNORM swapchain 直接寫入 shader 的線性光照結果。這是新增原版光照後背景泛白的可確認原因；不應降低材質或燈光常數來遮掩。

SDL 現在只對 **3D 模型貼圖**使用 `R8G8B8A8_UNORM_SRGB`；2D UI 的貼圖載入不變。模型 shader 在 swapchain 為 UNORM 時，對最終線性 RGB 做 sRGB encoding；若裝置提供 sRGB swapchain，交由 GPU target 自動轉換。這保留原版材質和三盞方向光數值。`MeshRenderer` 的 Boss 預覽同狀態擷取：[修正前](../build/material-parity-cull-boss.bmp)／[修正後](../build/material-srgb-encoded-boss120.bmp)。修正後天空從青白回到較深藍色，城市面不再整片發白；此比較只證明 SDL 修改前後，不等於原版逐像素驗收。Title 的 [修正後畫面](../build/material-srgb-title.bmp) 與修改前 SHA-256 完全相同。

追加修正後 Debug／Release 均已重新建置，各 27／27 CTest 通過；兩種組態各九種 30-frame GPU smoke test 全部通過，Boss 121-frame 擷取也成功。`git diff --check` 沒有空白錯誤。其他既有未提交修改保持原狀。

目前原版執行檔可以由 `PrintWindow` 擷取 Title，但自動化腳本無法穩定取得原版視窗焦點；後續按鍵未進入原版遊戲。該腳本的教學／戰鬥檔案因此**不能**作同狀態畫面證據。本輪已核對現版原版的材質實際載入與輸出色彩空間，並消除 SDL 內部前後畫面的過亮；原版與 SDL 在相同相機／tick 的戰鬥畫面、半透明晶體與加法特效仍待可重播錄影驗證。尤其 SDL 在 UNORM swapchain 內手動編碼後才做 alpha／additive blending，與原版 sRGB render target 的線性空間混色不保證完全相同；未經畫面證據，不調整透明度或效果亮度。

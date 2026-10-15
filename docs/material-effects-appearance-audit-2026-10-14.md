# 材質與特效最終外觀對照（2026-10-14）

## 判讀範圍與證據等級

這份文件只報告差異；本輪沒有修改遊戲程式、設定或素材。原版維持唯讀。原版以 `KamataEngine20260622/DirectXGame` commit `89de23714402d5168e11482463e03c63c0e00338` 及 2026-09-26 的 `提出/1_Game/BiggerFISTs.exe` 為準。SDL 版以 commit `1a28a7d01bb83dc4793451d19bbe82a2b9ec6579` 加上本輪開始前已有的 24 個未提交路徑為準。原版 EXE SHA-256：`38C90496A1883FA537F6B7B8A754E962615A8F1516D181ADC7766E4391DA0211`。

報告中的狀態分為：**畫面已對照且一致**、**確認有差異**、**使用者已決定保留的差異**、**缺少可比畫面**。「確認有差異（程式）」代表兩版的繪圖路徑確實不同；它不等於已測出該差異在原版畫面上的亮度或像素範圍。

本輪可以操作 SDL 固定 tick 截圖，也可啟動並截取目前原版的 Title。Windows 焦點未交給原版視窗：`AppActivate` 回報成功，但 foreground window handle 仍不是遊戲視窗；`PostMessage` 與全域鍵盤事件均未讓原版離開 Title。因此**原版戰鬥、開機與特效的同狀態截圖及並排短片尚未取得**。原版 2026-08-27 投稿截圖早於目前執行檔，沒有當作像素基準。SDL 1600×900 只用於縮放／裁切檢查；原版固定 1280×720，不做放大後的逐像素比較。

## 已完成的實際畫面和檔案檢查

| 對象 | 結果 | 證據 |
| --- | --- | --- |
| Title 1280×720 | **畫面已對照且一致（構圖與素材）**。目前 SDL 與原版 Title 有 943／921,600 像素不同（0.1023%）；每色頻平均絕對差 0.0046。這不宣稱逐像素完全相同。 | [原版 Title](../build/original-title-prompt-1280.png)、[本輪 SDL Title](../build/visual-parity-sdl-title.bmp)。原版截圖來自目前 2026-09-26 執行檔的先前成功擷取；本輪重新啟動該 EXE 確認仍停在同一 Title。 |
| 原版 3D 素材複製 | 36 個 `CombatScene::Initialize` 明文載入的 OBJ 目錄均在 SDL 專案內。SDL `assets/models` 的 38 OBJ、37 MTL、52 PNG 共 127 檔，與原版發行目錄相同相對路徑的檔案中 126 檔 SHA-256 相同。唯一不同的是 `cockpit/cockpit.mtl`，SDL 將原版不存在的 `NEW/cockpitTexture.png` 改成副本內有效的 `cockpitTexture.png`。**檔案一致不等於渲染一致。** | `config/battle-assets.cfg`、`DirectXGame/Game/CombatScene.cpp:1249-1285`、兩版 `cockpit.mtl`。 |
| 2D 圖片 | SDL 的 7 個 UI PNG 和 8 個劇情 PNG 均可依 SHA-256 對應原版發行素材。Title 實際畫面已比對；劇情與 Ending 仍缺原版同狀態畫面。 | [SDL Title 1600×900](../build/visual-parity-sdl-title-1600.bmp)、[SDL Ending tick 870](../build/visual-parity-sdl-flow-870.bmp)。 |
| SDL 1600×900 | 本輪 Title、Boss 預覽與護盾截圖均為實際 1600×900；可見背景填滿畫面，沒有左右未繪製帶。這是 SDL 縮放檢查，**不是**原版外觀驗收。 | [Title](../build/visual-parity-sdl-title-1600.bmp)、[Boss](../build/visual-parity-sdl-boss120-1600.bmp)、[護盾](../build/visual-parity-sdl-shield750-1600.bmp)。 |

## 材質與繪製路徑差異清單

| 項目與狀態 | 原版／SDL 依據 | 實際觀察與影響 | 建議順序 |
| --- | --- | --- | --- |
| **確認有差異（程式）：模型光照** | 原版發行版 `Resources/EngineShaders/ObjVS.hlsl` 把法線變換至世界座標，`ObjPS.hlsl` 計算 wrap diffuse、Blinn–Phong specular，以及啟用的平行／點／聚光燈。SDL `ObjLoader` 保留法線和 MTL 的 `Ka/Kd/Ks`，但 `MeshRenderer.cpp:227-244,520-525` 只把位置、UV、`Ka` 與 opacity 傳給 `triangle.vert/frag.hlsl`；fragment shader 回傳貼圖 × tint × `Ka`。 | 影響場地、拳頭、座艙、敵人、Boss 與使用模型的特效的明暗和光澤。實際差值尚無原版戰鬥截圖可量測。 | **P1**：後續修正時先對齊照明，再微調個別模型顏色。 |
| **確認有差異（程式）：火焰混色／深度** | 原版 `main.cpp:28-41` 的一般世界繪製包含 `Player::Draw`、`RobotArm::Draw`、`EnemyProjectile::Draw` 與 `ChargeUnit::Draw` 的火焰；加法層只呼叫 `CombatScene::DrawEffects`（護盾／爆炸）。SDL `BattleScenePresenter.cpp:287-288,479-480,545-546,688-689` 將 Rocket、Ultimate、導彈與 Charge 火焰指定為 `AdditiveEffect`；`MeshRenderer.cpp:358-361` 以加法、讀取深度且不寫入深度繪製。 | 火焰與背景疊色、亮度和遮擋規則確實不同。現有 [Rocket tick 56](../build/visual-parity-sdl-rocket56.bmp) 顯示 SDL 單邊狀態，仍需原版同動作短片判斷可見程度。 | **P1**：日後先恢復原版火焰繪製層，再比較顏色／尺度。 |
| **確認有差異（程式）：世界模型剔除** | 原版 `Object3d::PreDraw` 預設 `CullingMode::kBack`；透明背景另外指定 back culling。SDL `MeshRenderer.cpp:248` 的各模型 pipeline 使用 `SDL_GPU_CULLMODE_NONE`。原版特效與第一人稱層本來就使用 none。 | 開放或薄片模型可能多畫背面；對封閉模型的可見影響尚待同畫面核對。 | **P2**：逐類比較，避免把應雙面顯示的特效一起改掉。 |
| **程式規則已對照；缺少可比畫面：護盾／爆炸混色** | 原版 `main.cpp:32-33` 的 `kAdd`、read-only depth 與 SDL `MeshRenderer.cpp:213-220,359-360` 的加法層一致。原版發行版 `ObjPS.hlsl` 和 SDL fragment shader 均對接近零的 alpha 做 `clip`，這項**不是**已確認差異。 | SDL [護盾 tick 750](../build/visual-parity-sdl-shield750.bmp)、[Spread Burst tick 660](../build/visual-parity-sdl-spread-660.bmp)、[675](../build/visual-parity-sdl-spread-675.bmp)、[685](../build/visual-parity-sdl-spread-685.bmp) 可見特效生命週期的單邊樣本；原版亮度、煙霧可見度及遮擋仍待驗。 | **P2**：取得原版同事件的短片，按事件後時間而非單靠遊戲 tick 比較。 |
| **缺少可比畫面：取樣、色彩空間與晶體透明度** | SDL 模型使用 linear min/mag sampler、RGBA8 UNORM 貼圖；晶體 PNG 有透明像素（例如 `crystleCube/cube.png` 左上 alpha 0、中心 255）。原版實際 sampler／輸出色彩空間沒有足夠實機證據。 | 背景改變晶體觀感屬透明貼圖可預期現象；目前不能判定 SDL 比原版更明顯。 | **P2**：同鏡頭、同晶體位置量測再決定是否需要調整。 |
| **使用者已決定保留的差異：黑幕與護盾語意** | 正式開機黑幕已依要求設為 0.75，而原版完整開機為 0.94；緊急停機最暗 0.65。SDL 護盾顏色依受擊後剩餘層數決定，符合使用者對藍／黃／紅的規則。 | 不列為待修的原版偏差；報告仍需保留這些例外以避免後續「校正」覆蓋。 | 保留。 |

## 特效逐事件對照

下表的「數值／事件已對照」來自原版 `ExplosionEffectManager`、`ShieldHitEffectManager`、`CombatScene`、`RobotArm` 等來源與 SDL `CombatSession`、`BattleScenePresenter`；它表示程式流程有對應實作，**不表示畫面已驗收**。

| 原版事件與 SDL 樣本 | 程式對照結果 | 畫面狀態與待查項目 |
| --- | --- | --- |
| 教學、練習與正式戰鬥的護盾受擊；[SDL tick 750](../build/visual-parity-sdl-shield750.bmp) | 受擊事件、跟隨玩家、0.10 秒展開＋0.16 秒保持＋0.20 秒淡出及模型選擇已接入；一般攻擊和直接破盾有測試。 | **缺少可比畫面**：原版同層數／同相機的顏色、亮度、六角形範圍。 |
| 玩家 HP 受傷紅色覆蓋；[SDL tick 770](../build/migration-baseline-20261013/needle-770.bmp) | SDL 只在 HP 實際下降時顯示；護盾吸收時不顯示。 | **缺少可比畫面**：原版實際 alpha、覆蓋次序與持續時間。 |
| Punch／Rocket／Ultimate 火焰；[SDL Rocket tick 56](../build/visual-parity-sdl-rocket56.bmp) | 位置、返回／接回狀態已有來源對照；**繪製層確認不同**，見上表。 | **缺少可比影片**：火焰輪廓、顏色、尾跡與取消時點。 |
| MissileUnit 導彈與 ChargeUnit booster；[SDL MissileUnit](../build/visual-parity-sdl-missile-unit-120.bmp)、[ChargeUnit](../build/visual-parity-sdl-charge-unit-120.bmp) | 模型及火焰素材已載入；火焰使用加法層的差異同上。 | **缺少可比影片**：飛行中火焰比例、可見時間與遮擋。 |
| Bomber／浮空炸彈；[SDL Bomber](../build/visual-parity-sdl-bomber-120.bmp) | 模型素材與生成流程已接入；投射物命中後走爆炸事件。 | **缺少可比影片**：攜帶炸彈、拋出、命中與爆炸位置。 |
| 投射物命中／攔截、敵人擊倒終點、Boss 核心命中、Spread Burst 的 flash／fire／smoke；[SDL 660](../build/visual-parity-sdl-spread-660.bmp)／[675](../build/visual-parity-sdl-spread-675.bmp)／[685](../build/visual-parity-sdl-spread-685.bmp) | 原版與 SDL 都有 0.12 秒 flash、0.38 秒 fire、延遲 0.12 秒開始的 0.85 秒 smoke；比例與 tint 的原始數值相對應。 | **缺少可比影片**：原版同事件中心、火光亮度、煙霧是否顯著。SDL tick 685 樣本肉眼看不到明顯煙霧，尚不能判定為原版差異。 |
| Boss 晶體、核心／部件、擊倒小爆炸與最後爆炸；[SDL Boss tick 120](../build/visual-parity-sdl-boss120.bmp) | 狀態與事件已接入；死亡演出結束後才轉場有測試。 | **缺少可比影片**：晶體與背後場景的混色、各模式光澤、死亡最後爆炸完全顯示。 |

## 驗證與剩餘證據

- 本輪重新建置 Debug、Release 全部目標，兩組 CTest 均 **27／27 通過**；完整遊戲、兩個第一人稱模式與六個戰鬥預覽，在 Debug／Release 共 18 次 30-frame GPU smoke test 全部通過。這些測試只證明程式可執行、狀態與模型可繪製。
- 本輪額外擷取 Title、開機 tick 120、座艙、五類敵人／波次預覽、Boss tick 120、Rocket tick 56、護盾 tick 750、Spread Burst tick 660／675／685 及 Ending；其中 Title、Boss、護盾另擷取 1600×900。暫改的 `build/Debug/display.cfg` 已逐位元組還原。重播腳本是 [Spread Burst 測試輸入](../build/visual-parity-boss-spread.replay)。
- **沒有並排動作短片**：目前可取得的原版實機畫面只到 Title，原版視窗未收到鍵盤焦點。要完成視覺驗收，需原版 1280×720 錄影，至少涵蓋 Rocket 飛出／返回、三色護盾、一般爆炸三段、Missile／Charge 火焰、Boss 晶體與死亡最後爆炸；每段保留事件前後約 2 秒及同一相機方向。取得影片後再製作並排短片與差異標記。
- 目前可確定的優先修正候選為 **模型光照**與**火焰繪製層**。世界模型背面剔除列第二批。其他亮度、色彩空間、晶體與煙霧觀感，必須先取得同狀態原版畫面；此次沒有修改它們，也沒有把未驗畫面宣稱為一致。

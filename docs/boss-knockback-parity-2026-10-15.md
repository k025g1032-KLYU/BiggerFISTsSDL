# Boss 受擊擊退核對（2026-10-15）

## 原版規則

原版 `DirectXGame/Game/CombatScene.cpp` 在普通拳、Rocket Punch、Ultimate 成功傷害 Boss 核心後，呼叫 `Boss::ApplyKnockback`。距離分別為 1.75、2.50、6.0；前兩者衰減 0.18 秒，Ultimate 衰減 0.75 秒。方向是 Boss 中心遠離命中時的拳頭／Ultimate 中心，僅在水平面計算。原版 `Boss::UpdateKnockback` 每個固定步長以剩餘時間比例降低速度，並使用戰場及 Base 的連續邊界判定。Ultimate 命中 Idle、Boxer 或 SpreadCounter 時，另使行為停頓 0.65 秒；正在執行的 Full Barrage 仍依原版優先順序進入。致命一擊改走擊倒飛行，不再套一般擊退。

## SDL 原狀與修正

修正前 `CombatSession::ApplyTargetDamageImpl` 會扣 Boss HP、播放回饋及產生爆炸，但 `BattleBoss` 沒有受擊擊退狀態，命中後不會因攻擊方向位移。普通拳及 Rocket Punch 現已加入固定更新衰減及邊界停止；其距離與時間寫入 `boss-combat.cfg`。Boss 的前後位置仍提供給顯示補間與移動目標碰撞，重置時清除擊退。

**2026-10-15 後續需求覆蓋 Ultimate 原版數值：**使用者指定 Ultimate 命中時，Boss 整個被帶到牆邊。因此 SDL 改為沿 Ultimate 飛行方向、以 Ultimate 飛行速度持續推動 Boss 至戰場邊界；普通拳與 Rocket Punch 不變。期間暫停 Boss 行為；致命一擊到牆後停留 0.20 秒，再開始原有死亡動作。這是刻意與原版的 6.0 單位／0.75 秒擊退不同，已移除 SDL 中不再使用的兩項 Ultimate 擊退設定。

## 驗證與界線

`BattleBossTests` 檢查普通拳及 Rocket Punch 的第一 tick 與位移、Ultimate 中斷 Boxer／SpreadCounter、沿斜線推到牆、致命一擊的死亡演出時機、戰場／Base 邊界及重置。`CombatSessionTests` 檢查正常命中資料是否啟動各種擊退，並驗證 Ultimate 一直推到牆。`BattleReplayTests` 與 `FullGameIntegrationTests` 驗證輸入驅動的 Boss 擊敗與完整流程。

Debug 與 Release 建置成功，兩種組態的 CTest 各 27／27 通過。Boss 戰預覽在兩種組態均通過 120 幀 GPU smoke test；完整遊戲在兩種組態均通過 30 幀 GPU smoke test。這些測試可確認數值、流程與基本繪製運作，不能代替原版同視角的擊退畫面對照。

自動玩家只追蹤 Boss，無法攔截晶體；加入擊退後，使用正式的 Base HP 10 時，它會先打倒 Boss，再於死亡演出期間因殘留晶體使 Base 歸零。輸入通關測試因此將**測試場**的 Boss 戰 Base HP 設為 20，未修改正式設定。Base 扣血、Game Over 優先順序仍有獨立測試。正式難度下的一輪人工操作與原版同視角的擊退畫面，尚未完成實機驗收；不以測試場通關宣稱正式平衡已確認。

## 死亡姿態與晶體顯示校正

原版 `Game/Boss.cpp` 的致命傷分支會停用攻擊狀態、清除晶體發射計時，並呼叫 `SetNeedlePose(Spread, false)`；`Update` 在死亡期間仍逐幀執行尖錐姿態補間。原版 `Draw` 在 `Defeating` 階段持續繪製尚未消耗、亦未處於重新裝填的晶體，轉為 `Defeated` 後才停止繪製整個 Boss。

SDL 現在於致命傷時清除待執行攻擊及晶體上升計時，將尖錐目標改為 `Spread`，固定更新仍持續補間；`CombatSession` 同時清除這一 tick 的 Boss 攻擊判定。呈現層在 `Defeating` 顯示可用晶體，在 `Defeated` 移除 Boss 模型。`BattleBossTests` 驗證從 Needle Combo 進入死亡時不瞬間跳姿態、貼牆期間逐步轉為 `Spread`，且死亡期間沒有新攻擊；`BattleReplayTests` 用練習場的 Full Barrage 指令保留可用晶體，驗證死亡開始時仍顯示晶體與攻擊判定立即清除。一般 Full Barrage 結束後晶體已消耗，因此正常路徑可能看不到這個差異。

**待原版同狀態畫面確認：**原版致命傷會將 `fullBarragePitch_` 立即歸零；SDL 目前從命中前的 root pitch 接續死亡旋轉，避免與 Ultimate 貼牆演出之間瞬間翻轉。原版的 Full Barrage 傷害鎖會在動作結束後才解除，正常戰鬥不能在俯仰途中擊倒 Boss；差異主要涉及練習場強制切換模式等測試操作。尚無這種同狀態的原版畫面，因此暫保留現狀，不宣稱外觀已對齊。Ultimate 貼牆本身是上述使用者指定的刻意差異。

本次校正後 Debug 與 Release 皆完成建置，兩種組態各 27／27 項 CTest 通過；Boss 預覽各繪製 120 GPU 幀、完整遊戲各繪製 30 GPU 幀並正常結束。這些結果不包含原版同鏡位的死亡姿態目視比對。

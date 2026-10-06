# MissileUnit 原版模型：兩種材質

原版 `CombatScene` 使用 `Resources/RocketLaunch/RocketLaunch.obj` 當作 MissileUnit 模型。這份 OBJ 有兩個 `usemtl` 區段，分別讀取 `Launcherv2.png` 與 `Launcher.png`。先前 SDL 版的 `ObjLoader` 要求所有材質指向同一張 PNG，因此無法直接載入。

本步驟將原版 OBJ、MTL 和兩張 PNG **複製**到 `assets/models/RocketLaunch/`。原版檔案沒有修改。`ObjLoader` 現在按 OBJ 中的材質切換建立 index 範圍；`MeshRenderer` 對每個範圍綁定相應的 PNG 再繪製。已有的單貼圖模型仍使用原本的載入方式，兩個材質若共用同一張 PNG 也只會在一個 GPU mesh 中載入一次。

執行模型預覽：

```powershell
& '.\build\Debug\BiggerFISTsSDL.exe' --missile-unit-model-preview
```

預覽會緩慢旋轉原版模型，以便檢查兩個材質。自動驗證可加上 `--smoke-test`：載入兩張 PNG、繪製 30 幀後自行結束。`ObjLoaderTests` 另外檢查兩個 index 範圍與錯誤處理。

目前只完成 **模型與貼圖的繪製**。下一步才會把原版 MissileUnit 的降落、起飛、巡邏、攻擊、受擊與死亡規則接到第一人稱戰場。

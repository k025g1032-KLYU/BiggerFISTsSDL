# 為場景物件加入唯一 ID

原本的 `scene.cfg` 只寫模型路徑、位置、縮放與旋轉。假設同一份 `Target.obj` 放在畫面三次，三個物件都使用同一份模型資源；只靠 OBJ 檔名無法指定「哪一個 Target」。現在每個 `model` 區塊都必須有一個唯一 `id`。

## 怎麼寫

預設場景中的兩個物件是：

```text
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

`model` 指定要讀取哪份 OBJ；`id` 是這個**場景物件**的名稱。將另一個 Target 放入場景時，可以重用 `Target/Target.obj`，但要換成不同的 `id`，例如 `target_2`。名稱需以英文字母或 `_` 開頭，後面可使用英文字母、數字、`_` 或 `-`；同一份 `scene.cfg` 中不可重複，大小寫會區分。少寫 `id`、格式錯誤或重複時，程式會回報檔名與行號並停止啟動。

## 程式如何使用 ID

`SceneConfig` 先讀取並檢查每個 `id`。`SceneLoader` 載入 OBJ 時，把 `id` 複製到對應的 `ModelInstance`。`FindModelById(scene, "target")` 可以從 `ModelScene` 取得指定物件；找不到時會回傳 `nullptr`。這比使用 `models[0]` 更穩定，因為調整設定檔中的物件順序時，`models[0]` 可能變成另一個物件。

`meshIndex` 的用途不同：它指向 GPU 使用的 mesh 資源。兩個 instance 可以有不同 `id`，但共用同一份 OBJ、PNG 與 `meshIndex`。場景物件的 `id` 目前沒有自動賦予「玩家」或「敵人」規則。

## 驗證與接下來的用途

`SceneConfigTests` 檢查缺少、無效與重複的 `id`；`SceneLoaderTests` 檢查三個 instance 重用兩份 mesh、能依 `id` 找到正確 instance，且程式建立的重複 `id` 也會被拒絕。CTest 8 項全部通過，主程式的單模型及多模型 smoke test、獨立多模型範例也各成功繪製 30 幀。

這一步只建立穩定的物件身分。原版玩家是有身體、雙臂、鏡頭及獨立戰鬥狀態的複合物件；目前的 `LfistTEST` 只是其中一個美術模型，因此不把它直接標成整個玩家，也不讓 WASD 移動它。玩家物件及其控制需要後續依原版遊戲結構逐步移植。

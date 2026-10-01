# BiggerFISTs SDL3：第一個視窗

這是移植的第一個小步驟。執行後會出現 1280×720 的深藍色視窗。按 `Esc` 或視窗右上角的 `X` 即可關閉。

目前只確認 SDL3 專案可以建置、開視窗與正常退出；遊戲模型和戰鬥程式尚未移植。`src/main.cpp` 暫時使用 SDL3 的簡易 2D renderer 清除背景，之後的 3D 階段會改用 SDL GPU。

## 專案檔案

- `src/main.cpp`：程式入口、視窗與事件處理。
- `CMakeLists.txt`：告訴 CMake 如何編譯程式並連結 SDL3。
- `vendor/SDL`：固定在 SDL3 `release-3.4.16` 的官方原始碼。
- `build`：CMake 產生的檔案；可以重新產生，不需要手動編輯。

這個專案不會讀取或修改原本的 KamataEngine 專案。

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

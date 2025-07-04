# ReShade アドオン開発プロジェクト カスタム指示

🎭 **開発哲学**

ReShade API と C++17 を使用した環境下で、モダンなC++開発手法とアドオンの安定性・保守性に重きを置いたルールを定めることで、開発者のモチベーション向上と技術的成長を促進するとともに、ReShadeユーザーにとっても安定性と使いやすさを提供し、双方にとって価値のある、将来長きに渡って使い続けられる良いアドオンを作り上げることができます。

---

## 🤖 **AI判断支援・優先度ガイド**

### 📊 優先度マーキング

このカスタム指示では、以下の優先度マーキングを使用します：

🔴 **最重要** - 必ず遵守（プロジェクト品質の根幹）
🟡 **重要** - 可能な限り遵守（品質・保守性向上）
🟢 **推奨** - 改善機会があれば適用（さらなる品質向上）

### 🧠 AI判断フローチャート

#### パフォーマンス vs 可読性の判断

```
リアルタイム描画処理？ → YES → 🔴 効率的なAPIコール、メモリ最適化
                    → NO  ↓
UI/設定処理？ → YES → 🟡 可読性優先、適切なUI実装
            → NO  ↓
一回限りの初期化処理？ → YES → 🟢 可読性優先、保守性重視
```

#### 例外処理の詳細度判断

```
ReShade API エラー？ → 🔴 具体的メッセージ + エラーコード
ユーザー設定エラー？ → 🟡 わかりやすい説明 + 解決手順
システムエラー？ → 🟢 ログ出力 + ユーザーフレンドリー
```

#### 多言語対応の優先度

```
UI表示テキスト？ → 🔴 _(TEXT)マクロで多言語対応
エラーログ？ → 🔴 英語で記述（技術的情報共有）
開発者コメント？ → 🟡 日本語推奨
```

---

## 🔥 最高優先ルール（他の全ルールより優先）

### 🔴 エラー多発時の自動停止ルール

- AIや自動化ツールは、同じエラーや修正不能なエラーが複数回（2回以上）連続して発生した場合、処理を自動的に停止し、ユーザーに状況を報告すること。
- エラーが続く場合は、無限ループや無駄なリトライを避け、ユーザーの指示を待つこと。
- このルールは他の全カスタム指示よりも優先して適用する。

### 🔴 C++17/ReShade API制約ルール

- このプロジェクトは**C++17/Visual Studio 2017以降**を対象とする。
- **C++17を超える機能**（C++20以降の機能）の使用は以下の場合を除いて禁止：
  - 既存コードで既に使用されている場合
  - 新規プロジェクトや単一ファイル追加の場合
  - 上記以外の場合は**必ずユーザーの事前承諾を得る**こと
- **ReShade APIのバージョン制約**：RESHADE_API_VERSION 17 に準拠し、非推奨APIの使用は避ける
- 実装提案や修正提案時は、使用する機能のC++17標準およびReShade APIバージョンを明示すること。

### 🔴 ReShade API実装参照ルール

- **ReShade APIの実際の動作確認**が必要な場合は、以下の参照先を使用すること :
  - **第一参照**: `deps/reshade/` フォルダ内の完全な実装
  - **第二参照**: GitHub の `crosire/reshade` リポジトリ（実際の参照先）
- **API使用時の注意事項** :
  - 不明な API の動作は必ず実装を確認してから使用する
  - 新しい API を使用する場合は、対応するReShadeバージョンを確認する
  - 非公開APIや内部実装への依存は避ける

---

## 🎯 言語仕様・構文ルール（基本原則）

### 🔴 C++17 現代的構文統一ルール

#### 基本原則

- **if constexpr**: コンパイル時条件分岐を積極的に使用
- **構造化束縛**: `auto [key, value] = map.insert(...)`等の可読性向上
- **std::optional**: null許容型の明示的表現
- **std::variant**: 型安全な共用体の使用
- **範囲for**: 可能な限り範囲ベースfor文を使用

#### 適用例

```cpp
// ✅ 推奨: C++17現代的構文
if constexpr (std::is_same_v<T, int>) {
    // コンパイル時分岐
}

// 構造化束縛
auto [iterator, success] = map.insert({key, value});

// 範囲for
for (const auto& [key, value] : configuration_map) {
    // 処理
}

// ❌ 古い記法: C++11以前の書き方
if (typeid(T) == typeid(int)) {
    // 実行時型チェック（避ける）
}
```

### 🔴 RAII・リソース管理

- **スマートポインタ**: `std::unique_ptr`、`std::shared_ptr`の積極的使用
- **RAII**: リソースの自動管理を徹底
- **例外安全**: 強い例外安全性の保証

```cpp
// Forward declaration or typedef
class resource_type {
public:
    // Resource implementation
};

// ✅ 推奨: RAII原則
class adjustdepth_config {
private:
    std::unique_ptr<resource_type> resource;

public:
    adjustdepth_config() : resource(std::make_unique<resource_type>()) {}
    // デストラクタで自動的にリソース解放
};

// ❌ 避ける: 手動メモリ管理
class bad_config {
    resource_type* resource;
public:
    bad_config() : resource(new resource_type()) {}
    ~bad_config() { delete resource; } // 例外時にリークの危険性
};
```

---

## 📐 コーディング基準・規約

### 🔴 C++ コーディング規約準拠

- **Microsoft C++ コーディング規約**および**Google C++ Style Guide**に準拠
- **命名規則**:
  - クラス名: `snake_case` (例: `adjustdepth_config`, `screenshot_state`, `uibind_context`)
  - 関数名: `snake_case` (例: `load`, `save`, `reset`)
  - 変数名: `snake_case` (例: `config_path`, `error_occurs`)
  - プライベートメンバー変数: `snake_case` (trailing underscoreなし、例: `config_path`, `is_initialized`)
  - 定数名: `SCREAMING_SNAKE_CASE` (例: `MAX_SHORTCUTS`)
  - 静的メンバー変数: `snake_case` (例: `instances`)

#### アドオン命名パターン

ReShadeアドオンプロジェクトでは、**複合語は一語で扱う**命名パターンを使用します：

- **プロジェクト名**: `addon-{name}` 形式（複合語は一語化）
- **ディレクトリ名**: `src/addon-{name}/` 形式
- **メインファイル**: `{name}.cpp`, `{name}.hpp`

**✅ 正しいアドオン命名例**:
```
addon-fxlocalization    → FX Localization (fx + localization = 一語)
addon-adjustdepth       → AdjustDepth (adjust + depth = 一語)
addon-screenshot        → Screenshot (screen + shot = 一語)
addon-uibind           → UIBind (ui + bind = 一語)
```

**✅ クラス命名例**:
```cpp
// プロジェクト: addon-fxlocalization
// ディレクトリ: src/addon-fxlocalization/
// ファイル: fxlocalization.cpp, fxlocalization.hpp

class fxlocalization_config {          // アドオン名_機能名
    std::string config_path;           // snake_case (trailing underscoreなし)
    std::vector<localization_entry> entries;
public:
    std::error_code load_config(const std::filesystem::path& path);
    std::error_code save_config() const;
};

class fxlocalization_context {         // メインロジック
    fxlocalization_config config;
public:
    std::error_code initialize();
    void shutdown();
};
```

**❌ 避けるべきアドオン命名**:
```cpp
// 複合語をハイフンで分割
addon-fx-localization   // ❌ ハイフン分割
class fx_localization_config {  // ❌ アンダースコア分割
class FxLocalizationConfig {    // ❌ PascalCase
class FXLocalization {          // ❌ 大文字略語
```

```cpp
// ✅ 推奨: 統一された命名規則
class adjustdepth_config {
private:
    std::string config_path;            // プライベートメンバー: snake_case
    static constexpr int MAX_SHORTCUTS = 10;  // 定数: SCREAMING_SNAKE_CASE
    static std::unordered_map<reshade::api::effect_runtime*,
                              std::unique_ptr<adjustdepth_config>> instances;  // 静的メンバー: snake_case

public:
    void load_configuration(const std::string& path);  // 関数: snake_case
    bool is_valid() const { return !config_path.empty(); }
};

// ❌ 避ける: 不統一な命名
class AdjustDepthConfig {
    std::string ConfigPath;             // PascalCase (避ける)
    static const int max_shortcuts = 10;  // 定数なのにlowercase (避ける)
public:
    void LoadConfiguration(const std::string& Path);  // PascalCase (避ける)
    bool IsValid() const { return !ConfigPath.empty(); }
};
```

### 🔴 多言語対応・国際化ルール

#### 基本方針
- **コミュニティでの情報共有を重視**: エラーログや技術的メッセージは英語で統一し、他言語話者との問題解決やコミュニティでの情報交換を促進する
- **UI表示は多言語対応**: `_(TEXT)`プリプロセッサを使用してWindowsリソース(.rc2)ファイルシステムによる翻訳対応
- **例外処理の最小化**: throwableなライブラリを避け、`std::error_code`を使用するオーバーロードを優先使用

#### 言語使用ルール

🔴 **英語で記述するもの**:
- **エラーログ・デバッグメッセージ**: `reshade::log_message`によるログ出力
- **例外メッセージ**: try-catchを極力避けるが、必要な場合は英語

🟡 **`_(TEXT)`プリプロセッサで多言語対応するもの**:
- **UI表示テキスト**: ImGuiオーバーレイで表示されるユーザー向けテキスト
- **設定項目名・説明**: ユーザーが直接操作する設定画面のテキスト
- **ボタンラベル・メニュー項目**: ユーザーインターフェース要素

🟢 **日本語で記述するもの**:
- **ユーザー向けドキュメント**: README、使用方法、トラブルシューティング
- **開発者向けコメント**: 実装意図や設計思想の説明
- **技術的コメント**: API使用方法、実装詳細、アルゴリズム説明などの技術的なコメント
- **コミット メッセージ・Issue報告**: GitHubでの情報共有
- **TODO・FIXME・HACK等**: 技術的な作業メモ
- **カスタム指示**: このドキュメント自体

#### 実装例

```cpp
// 必要なインクルード
#include <reshade.hpp>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <memory>
#include <vector>
#include <system_error>
#include <imgui.h>
#include "../share/localization.hpp"

// 型定義
struct shortcut_info {
    std::string name;
    std::vector<int> keys;
    std::string action;
};

// ✅ 推奨: 多言語対応システムの活用
void draw_adjustdepth_overlay(reshade::api::effect_runtime* runtime) {
    auto& config = adjustdepth_config::get_instance(runtime);

    // UI表示は多言語対応
    if (ImGui::Begin(_("Adjust Depth##adjustdepth"), &config.show_overlay)) {

        if (ImGui::CollapsingHeader(_("Shortcut Settings"))) {
            // UI要素は_(TEXT)で翻訳対応
            if (ImGui::Button(_("Add New Shortcut"))) {
                config.shortcuts.emplace_back();
                config.mark_modified();
            }

            // エラーチェックは英語でログ出力
            if (config.shortcuts.empty()) {
                reshade::log_message(reshade::log_level::warning,
                    "No shortcuts configured for AdjustDepth addon");
            }
        }

        // 設定項目も多言語対応
        if (ImGui::CollapsingHeader(_("Advanced Settings"))) {
            ImGui::Checkbox(_("Enable Auto Adjustment"), &config.auto_adjust_enabled);
        }
    }
    ImGui::End();
}

// ✅ 推奨: std::error_codeを使用してtry-catch回避
std::error_code load_configuration_file(const std::filesystem::path& path,
                                        adjustdepth_config& config) {
    // throwableな操作を避け、error_codeオーバーロードを使用
    std::error_code ec;

    if (!std::filesystem::exists(path, ec)) {
        if (ec) {
            reshade::log_message(reshade::log_level::error,
                "Failed to check file existence: " + ec.message());
            return ec;
        }

        reshade::log_message(reshade::log_level::info,
            "Configuration file not found, using defaults: " + path.string());
        return {}; // エラーではない
    }

    // ファイル読み込みもerror_codeベース
    std::ifstream file(path);
    if (!file.is_open()) {
        reshade::log_message(reshade::log_level::error,
            "Failed to open configuration file: " + path.string());
        return std::make_error_code(std::errc::permission_denied);
    }

    // 設定の解析処理...

    reshade::log_message(reshade::log_level::info,
        "Configuration loaded successfully from: " + path.string());
    return {};
}

// ❌ 避ける: 日本語ログメッセージ（コミュニティ情報共有が困難）
void bad_example() {
    reshade::log_message(reshade::log_level::error,
        "設定ファイルの読み込みに失敗しました");  // 避ける
}

// ❌ 避ける: 過度なtry-catch使用
void bad_exception_handling() {
    try {
        // throwableな操作
        std::filesystem::file_size("config.ini");  // 避ける
    } catch (const std::exception& e) {
        // 例外発生時の処理
    }
}
```

#### Windows リソース翻訳システムの活用

```cpp
// ✅ 推奨: 翻訳キーとして適切な英語テキストを使用
ImGui::Text(_("Depth adjustment value"));
ImGui::Button(_("Reset to Default"));
ImGui::Checkbox(_("Enable automatic depth detection"));

// tools/create_language_template.ps1によってWindowsリソースファイル(.rc2)が生成される
// 各言語の.rc2ファイルで翻訳を提供:
// - "Depth adjustment value" -> "深度調整値"
// - "Reset to Default" -> "既定値にリセット"
// - "Enable automatic depth detection" -> "自動深度検出を有効にする"
```

##### 翻訳ワークフロー

1. **テンプレート生成**: `tools/create_language_template.ps1`の実行
   ```powershell
   # 各アドオンプロジェクトのresフォルダに言語別リソースファイルを生成
   # 例: src/addon-screenshot/res/lang_ja-JP.rc2
   ```

2. **リソースファイル構造**:
   ```rc
   /////////////////////////////////////////////////////////////////////////////
   // Japanese (Japan) resources

   #if !defined(AFX_RESOURCE_DLL) || defined(AFX_TARG_JPN)
   LANGUAGE 17, 1

   /////////////////////////////////////////////////////////////////////////////
   //
   // String Table
   //

   STRINGTABLE
   BEGIN

   6257 "実行中の構成："         // 翻訳済み
   48369 "Depth adjustment value"  // 未翻訳（_(TEXT)の内容そのまま）

   END
   ```

3. **翻訳プロセス**:
   - **初期状態**: `_(TEXT)`の内容がそのまま出力される
   - **翻訳作業**: 翻訳者が各言語の.rc2ファイルを編集
   - **コミット**: 翻訳完了後、変更をコミットして翻訳を反映

4. **ファイル配置**:
   ```
   src/addon-{name}/res/
   ├── lang_ja-JP.rc2      // 日本語翻訳
   ├── lang_ko-KR.rc2      // 韓国語翻訳
   ├── lang_zh-TW.rc2      // 中国語（繁体字）翻訳
   └── ...                 // その他の言語
   ```

#### localization.hppシステムの理解

```cpp
// localization.hppの仕組み:
// 1. _(message) マクロがCRC16ハッシュを計算
// 2. reshade::resources::load_string<CRC16>()でリソースから翻訳文字列を取得
// 3. 翻訳が見つからない場合は元の英語テキストを返す

// ✅ 実装時の注意点
void display_error_dialog() {
    // UI表示: 多言語対応
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
        _("Error: Invalid configuration detected"));

    // ログ出力: 英語固定（技術的情報共有用）
    reshade::log_message(reshade::log_level::error,
        "Configuration validation failed: invalid depth range specified");
}
```

---

## 📁 プロジェクト構造・ファイル組織

### 🔴 ディレクトリ構造標準規則

ReShade アドオンプロジェクトは以下の標準構造に従う：

```
src/
├── addon-{name}/          # 個別アドオン（例: addon-adjustdepth）
│   ├── main.cpp          # アドオンのメインロジック
│   ├── config.cpp        # 設定管理
│   ├── config.hpp        # 設定管理ヘッダー
│   └── overlay.cpp       # UI/オーバーレイ実装
├── share/                # 共有コンポーネント
│   ├── localization.hpp  # 多言語対応システム
│   └── (その他共有ヘッダー)
└── Directory.Build.props # MSBuildプロパティ
```

### 🔴 アドオン構成規則

各アドオンは以下のファイル構成を持つ：

```cpp
// main.cpp - アドオンのエントリーポイント
#include <reshade.hpp>
#include "config.hpp"

extern "C" __declspec(dllexport) const char* NAME = "AddonName";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "Description";

// config.hpp - 設定管理クラス
class addon_config {
    static addon_config& get_instance(reshade::api::effect_runtime* runtime);
    void load();
    void save();
    void reset();
};

// overlay.cpp - UI実装
void draw_overlay(reshade::api::effect_runtime* runtime);
```

---

## 🏗️ 設計・API・アーキテクチャ

### 🔴 ReShade API設計パターン

#### イベントドリブンアーキテクチャ

```cpp
// ✅ 推奨: ReShade API v17+対応
static bool on_init(reshade::api::effect_runtime* runtime) {
    // 初期化処理
    auto& config = addon_config::get_instance(runtime);
    config.load();

    reshade::log_message(reshade::log_level::info,
        "Addon initialized successfully");
    return true;
}

static void on_destroy(reshade::api::effect_runtime* runtime) {
    // 終了処理
    auto& config = addon_config::get_instance(runtime);
    config.save();

    reshade::log_message(reshade::log_level::info,
        "Addon destroyed successfully");
}

// イベント登録
BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        if (!reshade::register_addon(hModule))
            return FALSE;

        reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init);
        reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy);
        break;
    case DLL_PROCESS_DETACH:
        reshade::unregister_addon(hModule);
        break;
    }
    return TRUE;
}
```

#### 設定管理パターン

```cpp
// ✅ 推奨: シングルトンパターンでランタイム別設定管理
class adjustdepth_config {
private:
    static std::unordered_map<reshade::api::effect_runtime*,
                              std::unique_ptr<adjustdepth_config>> instances;

    std::filesystem::path config_path;
    bool auto_adjust_enabled = false;
    std::vector<shortcut_info> shortcuts;

public:
    static adjustdepth_config& get_instance(reshade::api::effect_runtime* runtime) {
        auto it = instances.find(runtime);
        if (it == instances.end()) {
            auto config = std::make_unique<adjustdepth_config>();
            if (auto ec = config->initialize(runtime); ec) {
                reshade::log_message(reshade::log_level::error,
                    "Failed to initialize config: " + ec.message());
            }
            auto& ref = *config;
            instances[runtime] = std::move(config);
            return ref;
        }
        return *it->second;
    }

    std::error_code initialize(reshade::api::effect_runtime* runtime) {
        // ランタイムベースの設定ファイルパスを生成
        config_path = std::filesystem::current_path() / "adjustdepth_config.ini";

        // 既存の設定を読み込み
        return load();
    }

    std::error_code load() {
        std::error_code ec;
        if (!std::filesystem::exists(config_path, ec)) {
            if (ec) {
                reshade::log_message(reshade::log_level::error,
                    "Failed to check config file: " + ec.message());
                return ec;
            }
            reshade::log_message(reshade::log_level::info,
                "Config file not found, using defaults");
            return {};
        }

        // 設定読み込み処理...
        return {};
    }

    std::error_code save() {
        // 設定保存処理...
        return {};
    }
};
```

---

## ⚙️ 実装詳細・具体的技術

### 🔴 ReShade API実装ベストプラクティス

#### ImGui統合

```cpp
// ✅ 推奨: ImGuiとの適切な統合
static void on_reshade_overlay(reshade::api::effect_runtime* runtime) {
    auto& config = adjustdepth_config::get_instance(runtime);

    if (!config.show_overlay) return;

    // オーバーレイウィンドウの描画
    if (ImGui::Begin(_("Adjust Depth Settings##adjustdepth"),
                     &config.show_overlay,
                     ImGuiWindowFlags_AlwaysAutoResize)) {

        // 設定UI
        if (ImGui::CollapsingHeader(_("Shortcut Configuration"))) {
            draw_shortcut_settings(config);
        }

        if (ImGui::CollapsingHeader(_("Advanced Options"))) {
            draw_advanced_options(config);
        }

        // 設定リセット機能
        if (ImGui::Button(_("Reset to Defaults"))) {
            config.reset();
            config.mark_modified();
        }
    }
    ImGui::End();
}
```

#### パフォーマンス最適化

```cpp
// ✅ 推奨: 効率的なAPI使用
class performance_optimized_addon {
private:
    // フレームごとの処理を最小化
    mutable std::chrono::steady_clock::time_point last_update;
    static constexpr auto UPDATE_INTERVAL = std::chrono::milliseconds(16); // 60FPS

public:
    void on_present(reshade::api::effect_runtime* runtime) {
        auto now = std::chrono::steady_clock::now();

        // 必要な場合のみ更新
        if (now - last_update >= UPDATE_INTERVAL) {
            update_internal_state(runtime);
            last_update = now;
        }
    }

private:
    void update_internal_state(reshade::api::effect_runtime* runtime) {
        // 重い処理をここに集約
    }
};
```

---

## 🧪 品質保証・テスト

### 🔴 デバッグ・ログ出力指針

```cpp
// ✅ 推奨: 適切なログレベルと英語メッセージ
void addon_lifecycle_logging() {
    // 初期化成功
    reshade::log_message(reshade::log_level::info,
        "AdjustDepth addon initialized successfully");

    // 設定読み込み警告
    reshade::log_message(reshade::log_level::warning,
        "Configuration file not found, using default settings");

    // エラー発生時
    reshade::log_message(reshade::log_level::error,
        "Failed to save configuration: " + error_message);

    // デバッグ情報（開発時のみ）
    #ifdef _DEBUG
    reshade::log_message(reshade::log_level::debug,
        "Processing shortcut binding: " + shortcut_name);
    #endif
}
```

### 🔴 エラーハンドリング

```cpp
// ✅ 推奨: std::error_codeベースの堅牢なエラーハンドリング
class robust_addon {
public:
    std::error_code initialize(reshade::api::effect_runtime* runtime) {
        // 初期化処理（error_codeベース）
        if (auto ec = config.load(); ec) {
            reshade::log_message(reshade::log_level::error,
                "Configuration load failed: " + ec.message());
            return ec;
        }

        is_initialized = true;
        reshade::log_message(reshade::log_level::info,
            "Addon initialized successfully");
        return {};
    }

    std::error_code safe_operation() {
        // 操作前の状態チェック
        if (!is_initialized) {
            reshade::log_message(reshade::log_level::warning,
                "Operation attempted on uninitialized addon");
            return std::make_error_code(std::errc::operation_not_permitted);
        }

        // 安全な操作実行（error_codeベース）
        // ...
        return {};
    }

private:
    bool is_initialized = false;
    // ...
};
```

---

## 🏁 総合運用方針

### 🔴 開発フロー

1. **要件分析**: ユーザーニーズと技術的制約の明確化
2. **設計**: ReShade APIとの統合方法の検討
3. **実装**: 本カスタム指示に従った実装
4. **テスト**: 複数のReShade対応ゲームでの動作確認
5. **ドキュメント**: 多言語対応の説明書作成

### 🔴 品質保証

- **コードレビュー**: 本カスタム指示への準拠チェック
- **パフォーマンステスト**: フレームレート影響の確認
- **互換性テスト**: 複数のReShadeバージョンでの動作確認
- **セキュリティ確認**: 未定義動作や脆弱性の排除

### 🔴 継続的改善

- **ユーザーフィードバック**: 実際の使用感の収集
- **技術的負債の管理**: 定期的なコード最適化
- **API更新への対応**: 新しいReShade APIの活用
- **コミュニティ貢献**: オープンソースプロジェクトへの還元

---

> このカスタム指示は、ReShade アドオン開発における品質向上と効率化を目的として策定されました。継続的な改善と、コミュニティでの知識共有を通じて、より良いアドオン開発環境の構築を目指します。

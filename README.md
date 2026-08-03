# PG1KB Protype ZMK Firmware

PG1KB Proto (Page One Keyboard Prototype) のZMKファームウェア用モジュールです。

## 現在の実装範囲

- Seeed Studio XIAO nRF52840 Plus向けの `pg1kb_proto_right` シールド
- Seeed Studio XIAO nRF52840 Plus向けの `pg1kb_proto_left` シールド
- 5行 x 6列の左右キーマトリクス
- 右手側はPAW3222トラックボールをSPI接続のZMK pointing deviceとして有効化
- 左手側はPAW3222トラックボールをSPI接続のZMK pointing deviceとして有効化
- 両手とも overlay の `#define` で PMW3610 に切り替え可能
- 右手側のBAT_CHECKピンを単4アルカリ乾電池の残量測定用ADCとして使用
- 右手側（central）は ZMK Studio 対応
- 15分で Deep Sleep へ移行。キーの押下で復帰しない場合は、右手側のRESETボタンを押すことで復帰
- スクロールはスムーススクロール（HID Resolution Multiplier）＋慣性スクロールに対応

## キーマップ

![pg1kb_proto keymap](keymap-drawer/pg1kb_proto.svg)

キーマップ定義は `boards/shields/pg1kb_proto/pg1kb_proto.keymap` です。SVG は `keymap-drawer/sync-keymap-drawer.sh` で更新できます。

## ZMK Studio

[ZMK Studio](https://zmk.studio/) からキーマップをリアルタイムに書き換えられます。

### アンロック

Studio から変更を書き込むにはキーボードのアンロックが必要です。`&studio_unlock` は **レイヤー3の左手上段外側の左のキー**に割り当ててあります。

### 予備レイヤー

Studio では devicetree に定義されていないレイヤーを新規追加できません。そのため `pg1kb_proto.keymap` に `status = "reserved"` の予備レイヤーを2つ（`extra_1` / `extra_2`）用意してあります。通常ビルドでは無視され、Studio有効ビルドでのみ「追加できるレイヤー」として現れます。もっと必要なら同じ形式で追加してください。

### 注意

Studio でキーマップを管理し始めると、以降 `pg1kb_proto.keymap` を編集して書き込んでも反映されなくなります。ファイル側の変更を反映したいときは Studio の "Restore Stock Settings" を実行してください（予備レイヤーの追加だけは例外で、Studio側の設定を消さずに増やせます）。

Studio が編集できるのはレイヤーとキーへのビヘイビア割り当てだけです。コンボ（`combo-bs` / `combo-del` / `combo-enter`）、入力プロセッサ、`lt` のタッピング設定などは引き続き devicetree 側で管理します。

## トラックボールセンサーの切替

左右の overlay 先頭にある `#define PG1KB_TRACKBALL_PAW3222` / `#define PG1KB_TRACKBALL_PMW3610` のうち、使う方の `#define` だけを有効にします。`.conf` を触る必要はありません。

```dts
#define PG1KB_TRACKBALL_PAW3222
// #define PG1KB_TRACKBALL_PMW3610
```

PAW3222 と PMW3610 は同じ SCLK / SDIO / CS / MOTION 配線を使いますが、Devicetree のプロパティ名が異なります。

| | PAW3222 (`pixart,paw3222`) | PMW3610 (`pixart,pmw3610`) |
| --- | --- | --- |
| 解像度 | `res-cpi` | `cpi` |
| 必須 | `irq-gpios` | `irq-gpios`, `evt-type`, `x-input-code`, `y-input-code` |
| 非対応 | `cpi` / `evt-type` / `x-input-code` / `y-input-code` | `res-cpi` |

## トラックボールの役割

左右のボールはレイヤーごとに役割が変わります。入力プロセッサのチェーンは `pg1kb_proto.keymap` の末尾で定義しています。

| レイヤー | 左ボール | 右ボール | ねらい |
| --- | --- | --- | --- |
| Base | スクロール 1/2 + 慣性 | カーソル 3/2 | 左でスクロール、右でポインタ |
| Num | カーソル 3/2 | 精密カーソル 1/2 | 両手でポインタ操作 |
| Sym | 精密スクロール 1/6 + 慣性 | スクロール 1/2 + 慣性 | 両手でスクロール操作 |

「精密」は通常の 1/3 の速さです。`zip_*_scaler` は `<乗数 除数>` で余りを繰り越すので、分数倍でも動きは失われません。

スクロールとカーソル移動では Y の符号の意味が逆（`REL_Y` は正が下、`REL_WHEEL` は正が上）なので、そのボールの本来の役割と違う側に `Y_INVERT` を足しています。

### スムーススクロール

右手側（central）で `CONFIG_ZMK_POINTING_SMOOTH_SCROLLING=y` を有効にしています。HID Resolution Multiplier によってホストは wheel 16 単位を 1 ノッチとして扱うため、スクロールが段階的ではなく滑らかになります。HIDレポートを送るのは central 側だけなので、この設定も右手側の `.conf` にのみ書きます。

### 慣性スクロール

[zmk-input-processor-scroll-inertia](https://github.com/mjmjm0101/zmk-input-processor-scroll-inertia) により、ボールを弾いて離した後もiOS風に惰性でスクロールが続きます。

設定は `pg1kb_proto.dtsi` の `scroll_inertia_left_base` / `scroll_inertia_left_sym` / `scroll_inertia_right_sym` の3ノードです。

**このプロセッサは central 専用です。** HIDへ直接書き込む実装のため split peripheral ではコンパイルできず、左手ビルドに `status = "okay"` のノードが残るとモジュール側の `#error` でビルドが落ちます。一方で `pg1kb_proto.keymap` は左右共通で読まれるため、左手ビルドでも `&scroll_inertia_*` のラベル自体は解決できる必要があります。そこで **ノードは `pg1kb_proto.dtsi` に `status = "disabled"` で共有定義し、`pg1kb_proto_right.overlay` でのみ `okay` にする**という形をとっています（トラックボールの listener と同じ流儀）。

慣性の出力は後段のプロセッサを通らずHIDへ直接書き込まれるので、**プロセッサは `zip_scroll_scaler` の手前に置き、`scale` / `scale-div` をその scaler の2引数と一致させます。** ここがズレると、ボールを回している間のスクロール速度と慣性の速度が食い違います。

主な設定値（デフォルトから変更しているものだけ）:

| プロパティ | 値 | 意味 |
| --- | --- | --- |
| `axis` | `0` | 両軸フリー（2Dスクロール）。軸ロックはかけない |
| `layer` | `2`（Symの2ノードのみ） | このレイヤーがオフになった瞬間に状態をリセット |
| `scale` / `scale-div` | `1`/`2`、`1`/`6` | 後段の `zip_scroll_scaler` の引数と一致させる |
| `stop` | `2` / `3` | 慣性を打ち切る速度。切れ際が約 3.9 ノッチ/s に揃う値 |
| `start` | `25` | 発動に必要なピーク速度（弾く強さ）。デフォルト40 |
| `move` | `50` | 発動に必要な累積移動量（振り幅）。デフォルト80 |
| `min-events` | `6` | アームに必要なイベント数。デフォルト10では短いフリックが弾かれた |

左ボールのBaseだけ `layer` を指定していません。ここは専用スクロールレイヤーではなく listener のデフォルトチェーンだからです。そのためBaseでフリックした直後にSymへ切り替えると、慣性は `gesture-timeout` / 自然減衰 / `span` で止まるまで流れ続けます。

感触の調整は `start` / `move`（発動しやすさ）、`decay-fast` / `decay-slow` / `decay-tail`（尾の長さ）、`friction`（小さいフリックの止まり方）、`stop`（切れ際）で行います。詳細はモジュールの [README_ja.md](https://github.com/mjmjm0101/zmk-input-processor-scroll-inertia/blob/main/README_ja.md) を参照してください。

## 右手側ピン配置

物理配線ラベルは 1 始まりで、ファームウェア上の `row0` が `R1`、`col0` が `C1` に対応します。

| 機能 | 配線ラベル | XIAO Plusピン | ZMK/Devicetree 指定 | nRF52840ピン |
| --- | --- | --- | --- | --- |
| row0 | R1 | D15 | `&gpio0 10` | P0.10 |
| row1 | R2 | D14 | `&gpio0 9` | P0.09 |
| row2 | R3 | D13 | `&gpio1 1` | P1.01 |
| row3 | R4 | D12 | `&gpio0 19` | P0.19 |
| row4 | R5 | D11 | `&gpio0 15` | P0.15 |
| col0 | C1 | D6 | `&xiao_d 6` | P1.11 |
| col1 | C2 | D4 | `&xiao_d 4` | P0.04 |
| col2 | C3 | D3 | `&xiao_d 3` | P0.29 |
| col3 | C4 | D2 | `&xiao_d 2` | P0.28 |
| col4 | C5 | D1 | `&xiao_d 1` | P0.03 |
| col5 | C6 | D0 | `&xiao_d 0` | P0.02 |
| PAW3222 MOTION | - | D10 | `&xiao_d 10` | P1.15 |
| PAW3222 CS | - | D17 | `&gpio1 3` | P1.03 |
| PAW3222 SCLK | - | D18 | `NRF_PSEL(SPIM_SCK, 1, 5)` | P1.05 |
| PAW3222 SDIO | - | D19 | `NRF_PSEL(SPIM_MOSI, 1, 7)` / `NRF_PSEL(SPIM_MISO, 1, 7)` | P1.07 |
| BAT_CHECK | - | D5 / A5 | `io-channels = <&adc 3>`、BAT_CHECK–AIN3 間に 10kΩ 直列 | P0.05 / AIN3 |

## 左手側ピン配置

左手側は **row のピン順だけが右手と逆**（右手は row0=D15→row4=D11、左手は row0=D11→row4=D15）で、col とトラックボールの4信号は右手と同一です。

| 機能 | XIAO Plusピン | ZMK/Devicetree 指定 | nRF52840ピン |
| --- | --- | --- | --- |
| row0 | D11 | `&gpio0 15` | P0.15 |
| row1 | D12 | `&gpio0 19` | P0.19 |
| row2 | D13 | `&gpio1 1` | P1.01 |
| row3 | D14 | `&gpio0 9` | P0.09 |
| row4 | D15 | `&gpio0 10` | P0.10 |
| col0 | D6 | `&xiao_d 6` | P1.11 |
| col1 | D4 | `&xiao_d 4` | P0.04 |
| col2 | D3 | `&xiao_d 3` | P0.29 |
| col3 | D2 | `&xiao_d 2` | P0.28 |
| col4 | D1 | `&xiao_d 1` | P0.03 |
| col5 | D0 | `&xiao_d 0` | P0.02 |
| PAW3222 MOTION | D10 | `&xiao_d 10` | P1.15 |
| PAW3222 CS | D17 | `&gpio1 3` | P1.03 |
| PAW3222 SCLK | D18 | `NRF_PSEL(SPIM_SCK, 1, 5)` | P1.05 |
| PAW3222 SDIO | D19 | `NRF_PSEL(SPIM_MOSI, 1, 7)` / `NRF_PSEL(SPIM_MISO, 1, 7)` | P1.07 |
| BAT_CHECK | D5 / A5 | `io-channels = <&adc 3>`、BAT_CHECK–AIN3 間に 10kΩ 直列 | P0.05 / AIN3 |

左手側は `col-offset` を付けず（論理 col 0-5）、`ZMK_SPLIT_ROLE_CENTRAL=n` の peripheral です。物理的な左右反転はマトリクストランスフォームの `map` 側で吸収しています。

## 電池残量測定

左右それぞれに単4アルカリ乾電池があり、どちらも各 XIAO の `P0.05_A5_D5_SCL` ピン（AIN3）をBAT_CHECKとして使います。残量はBLE Battery ServiceでPC側へ報告します。

左手側は split peripheral なので、自分で測った残量を `zmk_battery_state_changed` イベントとして右手側（central）へ転送します（peripheral 側に追加の設定は不要）。右手側では `CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y` と `CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_PROXY=y` を有効にして、左手側の残量を追加のBattery Levelサービスとしてホストへ中継しています。ホスト側が複数バッテリーをどう扱うかはOS依存で、右手側のみ表示される場合があります。

残量計算には [zmk-feature-non-lipo-battery-management](https://github.com/sekigon-gonnoc/zmk-feature-non-lipo-battery-management) を使います。`pg1kb_proto_right.conf` では単4アルカリ向けに `1000mV = 0%`、`1500mV = 100%`、`1000mV` 以下を低電圧として設定しています。

## ライセンス

このモジュール自体は MIT ライセンスです。

依存コンポーネントのライセンスは以下の通りです：

| コンポーネント | ライセンス | 備考 |
| --- | --- | --- |
| [ZMK Firmware](https://github.com/zmkfirmware/zmk) | MIT | キーボードファームウェア本体 |
| [zmk-driver-paw3222](https://github.com/sekigon-gonnoc/zmk-driver-paw3222) | Apache-2.0 | PAW3222 トラックボールドライバー。元コードは Google LLC (Zephyr Project) 著作権、sekigon-gonnoc により改変 |
| [zmk-pmw3610-driver](https://github.com/badjeff/zmk-pmw3610-driver) | MIT | PMW3610 トラックボールドライバー。badjeff 著作権 |
| [zmk-feature-non-lipo-battery-management](https://github.com/sekigon-gonnoc/zmk-feature-non-lipo-battery-management) | MIT | 単4アルカリなど非LiPo電池向けの残量測定 |
| [zmk-input-processor-scroll-inertia](https://github.com/mjmjm0101/zmk-input-processor-scroll-inertia) | MIT | 慣性スクロールの入力プロセッサ。mjmjm0101 著作権 |

# badjeff/zmk-input-processor-cirque 導入検討

調査日: 2026-11-03 / 対象: [badjeff/zmk-input-processor-cirque](https://github.com/badjeff/zmk-input-processor-cirque) `main` (2276c49)

## 結論

| 要望 | 実現可否 | 備考 |
|---|---|---|
| 周辺部（リム）のスクロール | **できる** | このモジュールの主機能（edge-scroll ring / circular scroll） |
| 右クリック | **できる（ただし二本指ではない）** | リム部分を「タップ」すると右クリック（secondary tap） |
| 二本指タップで右クリック | **できない** | Pinnacle ASIC(1CA027) はシングルタッチ。absolute モードでも座標は1点(X/Y/Z)しか出ない。ソフトでもハードでも二本指は検出不可 |
| おまけ | できる | タップ→ドラッグ、慣性スクロール(フリック後の滑走)、着地/離陸時のブレ除去 |

**方針: 右クリックは周辺部タップで代替し、二本指タップは不要とする（2026-11-03 決定）。**

**ただし、導入には ZMK を `v0.3` → `main`（Zephyr 4.1）へ上げる必要があり、これが一番のコスト。**

## なぜ ZMK main が必須か

このプロセッサは **Zephyr 本家の `cirque,pinnacle` ドライバ**を `data-mode = "absolute"` で動かし、
`INPUT_ABS_X / ABS_Y / ABS_Z` を受け取る前提で作られている。

- Zephyr 本家の `drivers/input/input_pinnacle.c` は ZMK v0.3 の Zephyr 3.5 には**存在しない**（ZMK main の Zephyr 4.1 にはある）。
- 今使っている `geeksville/cirque-input-module`（toucan ブランチ）の absolute モードは
  - `ABS_Z` を送らない（タッチ中判定・慣性中断・lift filter が Z に依存しているので壊れる。特に「滑走中に指を置いても止まらず入力が捨てられる」）
  - 座標を 0..1024 にスケールして送る（プロセッサの中心既定値 1024/768 と合わない）

  ので、そのまま組み合わせるのは実用的でない。
- ZMK main では本家ドライバと geeksville モジュールが同じ `cirque,pinnacle` compatible を取り合うため、geeksville モジュールは外す必要がある。

## ZMK main 移行の影響（リスク）

1. **nice_view_gem のカスタムウィジェットが LVGL 8 API**（`lv_canvas_draw_img` 等）。ZMK main は LVGL 9 なのでビルドが通らない。
   本家 [M165437/nice-view-gem](https://github.com/M165437/nice-view-gem) の main は LVGL 9 対応済みなので、それをベースにカスタム部分を移植し直す必要がある。**作業量が一番大きい。**
2. **トラックパッドの省電力が少し落ちる可能性。** geeksville 版は ZMK の idle/sleep に合わせてパッドを止める独自コード（PM suspend/resume, idle sleeper）を持っているが、本家ドライバには無い。
   代わりに `sleep-mode-enable`（5秒無タッチでハードウェアスリープ、復帰に約300ms）が使える。
3. `zmk-rgbled-widget` は `v0.3` → `main` に変える。
4. 本家ドライバの `data-ready-gpios` の極性（説明文では「直結なら active low」）は実機で確認が必要。

## 構成案（split: 右=トラックパッド付きペリフェラル、左=セントラル）

プロセッサは **ペリフェラル（右）側で走らせる**のがよい。タップ判定（120ms）や
ドラッグ保持判定がタイミング依存なので、BLE 越しの遅延/まとめ送りの影響を避けたい。
また ABS_X/Y/Z を毎フレーム送るより、変換後の REL だけ送る方が split の帯域も軽い。

仕組み:
- `glidepoint_raw`（右だけに置く input-split, reg 1）: 物理パッドのイベントを `&zip_cirque` に通す。
  `zip_cirque` は常に `STOP` を返すので、生データはセントラルには送られない。
- `glidepoint_split`（reg 0）: デバイスを `&zip_cirque`（仮想デバイス）にし、変換後の REL_X/Y・REL_WHEEL・ボタンだけを送る。

### config/west.yml

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: caksoylar
      url-base: https://github.com/caksoylar
    - name: badjeff
      url-base: https://github.com/badjeff
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: zmk-rgbled-widget
      remote: caksoylar
      revision: main
    - name: zmk-input-processor-cirque
      remote: badjeff
      revision: main          # 動作確認後はコミットハッシュで固定推奨
  self:
    path: config
```

### boards/shields/toucan/toucan_right.overlay（トラックパッド部分）

```dts
#include <input/processors/cirque.dtsi>

&spi0 {
    /* pinctrl / cs-gpios は現状のまま */
    glidepoint: glidepoint@0 {
        compatible = "cirque,pinnacle";
        reg = <0>;
        spi-max-frequency = <1000000>;
        status = "okay";
        data-ready-gpios = <&gpio0 2 GPIO_ACTIVE_HIGH>; /* 極性は要実機確認 */
        sensitivity = "2x";
        data-mode = "absolute";
        invert-x;                /* 旧 x-invert 相当 */
        // sleep-mode-enable;    /* 省電力を優先するなら */
    };
};

&zip_cirque {
    /* ハプティクスは無いので feedback-* は設定しない */
    edge-scroll-margin-h-pct = <25>;      /* 大きいほど中央(カーソル)領域が狭く、リムが太くなる */
    edge-scroll-margin-v-pct = <19>;
    edge-scroll-gesture-delay-ms = <135>; /* リムに置いて静止 → この時間でスクロールモード。0 で即時 */
    // secondary-tap-disable;             /* リムタップの右クリックが不要なら */
    // inertial-disable;                  /* 慣性が合わなければ */
};

/ {
    split_inputs {
        glidepoint_raw: glidepoint_raw@1 {
            compatible = "zmk,input-split";
            reg = <1>;
            device = <&glidepoint>;
            input-processors = <&zip_cirque>;
        };
    };
};

&glidepoint_split {
    device = <&zip_cirque>;
};
```

現状右側にある `pointing_input_listener`（ペリフェラル上の listener）は不要になるので削除する。

### boards/shields/toucan/toucan.dtsi（セントラル側 listener）

現状の `glidepoint_listener` はそのまま使える（`REL_WHEEL` と `BTN_TOUCH`/`BTN_1` は ZMK の input listener がそのまま処理する）。
ただし出力の単位が変わる（absolute の生カウント差分）ので、`zip_xy_scaler 250 100` は実機で調整し直す
（badjeff の例は `4 7` 程度）。レイヤー1/2での「パッド全体でスクロール」(`zip_xy_to_scroll_mapper`) も併用できる。

## 操作感のまとめ

- 中央をなぞる → カーソル移動（フリックすると滑走）
- 中央をタップ → 左クリック
- リム（外周）をタップ → 右クリック
- リムに指を置いて少し止める → スクロールモード → 円を描くように回すとホイール（縦のみ。横は Shift 併用）
- タップ直後にもう一度触れて保持 → ドラッグ

## 段階的な進め方（提案）

1. ZMK main へ移行（nice_view_gem の LVGL 9 移植、geeksville → 本家ドライバ、relative モードのまま）して、今と同じ動作が出ることを確認。
2. `data-mode = "absolute"` + `zip_cirque` を入れて、スクロール/タップ/右クリックを確認・調整。

1 と 2 を分けることで、問題が出たときにどちらが原因か切り分けやすい。

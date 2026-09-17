# Sasken AIML Project

TI TDA4VM（J721E EVM）上で、H.264動画を入力とするForward Collision Warning（FCW）システムを開発・検証したプロジェクトです。Ubuntu PCでソースコードの編集とビルドを行い、TIボード上でH.264デコード、SSDによる車両検出、ROI処理、Tracker、TTC計算、警告表示、警告音を実行します。

## 1. プロジェクトの目的

Pythonで作成したFCWの基準実装をCへ移植し、TI TDA4VMのAVP2物体検出アプリケーションへ統合することが目的です。PC上の検証だけでなく、組み込みボード上で実際に動作させることを目標にしました。

## 2. システム全体の流れ

```text
H.264 video
    ↓
Hardware H.264 decoder (v4l2h264dec)
    ↓
NV12 frames (YUV)
    ↓
AVP2 / TIDL / SSD object detection
    ↓
ROI filtering
    ↓
IoU-based tracking
    ↓
TTC calculation
    ↓
FCW decision
    ↓
Overlay display and audio alert
```

AVP2の物体検出処理はH.264ファイルを直接入力として受け取らず、NV12形式の画像フレームを入力とします。そのため、H.264をハードウェアデコーダーでNV12へ変換し、1フレームずつSSDへ入力します。

今回の入力動画は1280×720です。元の設定とバッファサイズが入力解像度に合っていなかったため、設定とoverlay bufferを調整しました。また、元のH.264ファイルにはデコードエラーがあったため、FFmpegで再エンコードしたファイルを使用して検証しました。

## 3. ハードウェアとソフトウェア

- 開発環境：Ubuntu Linux PC
- 実行環境：TI J721E EVM（TDA4VM）
- Cortex-A72：Linuxおよびメインアプリケーション
- C7x DSP / MMA：AI推論およびSSD物体検出
- Cortex-R5F：リアルタイム処理・システム制御
- Hardware Video Decoder：H.264デコード
- 主なSDKアプリケーション：`app_tidl_avp2`、`app_multi_cam_codec`

Ubuntu PCでソースコードをビルドし、生成された実行ファイル、設定ファイル、動画、モデルをTIボードへ転送して実行します。

## 4. 開発の流れ

1. Pythonで物体検出、ROI、Tracker、TTC、警告処理の基準実装を作成。
2. Ubuntu PCにTI RTOS SDKをセットアップし、SDKビルド環境を準備。
3. TIの`app_tidl_avp2`を確認し、AVP2の入力形式と処理構成を調査。
4. `app_multi_cam_codec`を参考に、`v4l2h264dec`によるH.264デコードを確認。
5. H.264をNV12フレームへ変換し、AVP2のSSD入力へ接続。
6. AVP2の入力を、元の3チャンネルから1チャンネルへ変更。
7. PythonのFCW処理をCモジュールへ書き直す。
8. CモジュールをAVP2へ接続し、TIボード上で段階的に検証。

各段階で単体の動作を確認してから次の処理へ進むことで、デコード、物体検出、TTC、overlayの問題を切り分けました。

## 5. Python版FCWプロトタイプ

`code/`には、PC上で動作確認するためのPython版があります。フレームごとに、次の処理を実行します。

```text
Object detection → ROI → ID tracking → TTC → FCW alert
```

`code/ssd_ttc.py`は初期の一体型コードです。`code/fcw/`には、detector、ROI、tracker、TTC、alert、alarm、visualizer、loggerなどに分割したモジュール版があります。C版の設計・動作確認では、主にこのモジュール構成を参考にしました。

## 6. H.264デコード

### 6.1 デコード確認用コード

`app_multi_cam_codec/`は、TI SDKのH.264デコード確認用アプリケーションです。GStreamerの`v4l2h264dec`を使用して、H.264動画をNV12形式へ変換します。

### 6.2 AVP2接続用デコーダー

統合スナップショット側の`app_tidl_avp2/avp_decode_module.c/.h`には、H.264ファイルから1フレームずつNV12データを取り出す処理があります。デコーダーの出力は、解像度やstrideを確認したうえでAVP2の入力バッファへコピーします。

## 7. AVP2 Integration

### 7.1 AVP2本体

`app_tidl_avp2/`には、AVP2の初期化、OpenVX graphの作成、TIDL/SSD処理、display処理などが含まれています。

### 7.2 C版FCWモジュール

`codeC/`には、Python版から移植したCモジュールがあります。

- `main_pre.c/.h`：フレームごとのFCW処理の呼び出し
- `avp_fcw_roi.c/.h`：ROIの定義、ROI内判定、ROI描画関連処理
- `fcw_tracker.c/.h`：IoUを用いた物体追跡
- `fcw_ttc.c/.h`：TTC（Time To Collision）の計算
- `fcw_alert.c/.h`：FCW警告の判定
- `fcw_alarm.c/.h`：警告音の制御
- `fcw_tidl_adapter.c/.h`：SSD検出結果とFCW処理の接続
- `fcw_types.c/.h`：FCW処理で共有するデータ型

`codeC/avp_fcw_tracker.c/.h`も存在しますが、現在の`app_tidl_avp2/concerto.mak`の`CSOURCES`には含まれていません。現在のroot側のビルドで使用されるTrackerは`codeC/fcw_tracker.c/.h`です。

### 7.3 表示と警告の仕様

- 緑のBBox：SSDによる通常の検出結果。ROI外でも表示されます。
- 黄色の線：ROIの範囲。
- 赤のBBox：ROI内でFCW条件を満たした場合に表示されます。
- 警告音：FCW警告が成立した場合に再生されます。
- TTC：ROI内の対象車両について計算します。
- IoU tracking retention：検出が一時的に途切れても追跡を保持するための処理です。

検証では、警告成立後の赤いBBoxを少なくとも15フレーム表示し、IoU trackingの保持フレーム数も5から30へ変更しました。

## 8. 検証結果

- TIボード上で1280×720のH.264動画をNV12フレームへ変換できることを確認。
- SSDによる車両検出と緑のBBox表示を確認。
- ROI内の車両についてTTCを計算できることを確認。
- TTCが閾値を下回ったとき、赤いBBoxと警告音が出力されることを確認。
- ROI外の車両についてTTCを計算しないことを確認。
- 動画入力からFCW判定までの処理をTIボード上で確認。
- H.264デコード、SSD検出、ROI、Tracker、TTC、FCW alertの各機能を段階的に検証。

## 9. 現在のファイル構成

```text
Sasken_AIML_Project/
├─ app_tidl_avp2/                 # root側のAVP2ビルド対象
├─ app_multi_cam_codec/           # H.264デコード確認用コード
├─ code/                          # Python版FCWプロトタイプ
├─ codeC/                         # root側のC版FCWモジュールと単体テスト
├─ app_tidl_avp2_0903/            # 9月3日前後のスナップショット
├─ app_tidl_avp2_0910_new/        # 旧0903_01から名称変更した最新の統合スナップショット
│  ├─ app_tidl_avp2/              # H.264/overlayを含むAVP2アプリ
│  └─ codeC/                      # 統合版が参照するC版FCWモジュール
├─ app_tidl_avp2_0904/            # 過去スナップショットの構成
├─ 0904_new/                      # 統合版の別スナップショット
├─ h264/                          # H.264入力動画
├─ model/                         # SSD/TIDLモデル関連ファイル
├─ mp4/                           # デモ動画・確認用動画
├─ output/                        # 処理結果・ログ
├─ reports/                       # レポート関連ファイル
└─ presentation_output/           # 発表資料などの生成物
```

## 10. 最新ファイルの判断方法

このプロジェクトには日付付きのコピーが複数あるため、フォルダ名だけで最新ファイルを判断しないでください。ビルド対象は、実際に使用するフォルダの`concerto.mak`に記載されている`CSOURCES`で確認します。

### 10.1 root側のビルド構成

現在のroot側では、次のファイルを入口として確認します。

| ファイル | 役割 |
|---|---|
| `app_tidl_avp2/main.c` | AVP2メイン処理 |
| `app_tidl_avp2/concerto.mak` | ビルド対象ソースの定義 |
| `app_tidl_avp2/config/app_avp2.cfg` | 実行設定 |
| `codeC/main_pre.c/.h` | FCW処理の中心 |
| `codeC/avp_fcw_roi.c/.h` | ROI処理 |
| `codeC/fcw_tracker.c/.h` | Tracker処理 |
| `codeC/fcw_ttc.c/.h` | TTC計算 |
| `codeC/fcw_alert.c/.h`、`fcw_alarm.c/.h` | 警告判定・警告音 |

現在のroot側`app_tidl_avp2/concerto.mak`は、`codeC`のFCWモジュールをビルド対象にしています。ただし、現在の記述では`app_tidl_avp2/avp_decode_module.c`と`avp_fcw_overlay_module.c`は`CSOURCES`に含まれていません。

### 10.2 H.264とoverlayを含む統合スナップショット

H.264デコードとFCW overlayまで含む統合版は、次のスナップショット側で確認できます。

```text
app_tidl_avp2_0910_new/app_tidl_avp2/
0904_new/app_tidl_avp2/
```

これらの`concerto.mak`には、`avp_decode_module.c`、`codeC`のFCWモジュール、`avp_fcw_overlay_module.c`が記載されています。

`app_tidl_avp2_0910_new`は、以前の`app_tidl_avp2_0903_01`から名称変更したフォルダで、現在のH.264/overlay統合版として扱います。このフォルダには`app_tidl_avp2/`と、そのビルドから参照される`codeC/`が含まれています。`0904_new`は別の過去スナップショットとして残しています。一部の`main.c`、ROI、TTC、設定ファイルには差分があるため、実際にTIボードで使用した実行ファイルは、Ubuntu PC上のコミット履歴、更新日時、SHA256で最終確認してください。

### 10.3 実行ファイル

`vx_app_tidl_avp2.out`はソースではなく、ビルドによって生成される実行ファイルです。SDKビルド後の代表的な出力先は次のとおりです。

```text
vision_apps/out/J721E/A72/LINUX/release/vx_app_tidl_avp2.out
```

TIボードへ転送する前に、Ubuntu PCとボードの両方で次のコマンドを実行し、SHA256が一致することを確認します。

```bash
sha256sum /path/to/vx_app_tidl_avp2.out
```

### 10.4 過去版の扱い

`app_tidl_avp2_0903`、`app_tidl_avp2_0904`、`0904_new`は、比較やデバッグのために残している過去スナップショットです。`app_tidl_avp2_0908`は削除済みのため、現在の構成には含まれません。通常の修正・ビルドでは、最新の統合版である`app_tidl_avp2_0910_new`を使用し、`app_tidl_avp2_0910_new/app_tidl_avp2/concerto.mak`、`main.c`、同フォルダ内の`codeC/`をセットで扱ってください。

## 11. ビルド・実行の基本手順

1. Ubuntu PCで、最新の統合版`app_tidl_avp2_0910_new/`を使用するか、root側の`app_tidl_avp2/`と`codeC/`を使用するかを決める。
2. `concerto.mak`の`CSOURCES`に必要なソースが含まれていることを確認する。
3. RTOS SDK BuilderでAVP2アプリケーションをビルドする。
4. 生成された`vx_app_tidl_avp2.out`の更新日時とSHA256を確認する。
5. 実行ファイル、設定ファイル、動画、モデルをTIボードへ転送する。
6. ボード上で環境を初期化し、`run_app_tidl_avp2.sh`を実行する。
7. HDMI画面、BBox、ROI、TTCログ、FCW警告音を確認する。

入力動画、モデル、SDKのインストール先、ボード上の配置先は環境によって異なります。実行前に`app_avp2.cfg`とボード側の実行スクリプトを確認してください。

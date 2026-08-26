"""設定値をまとめたモジュール。

対応する資料の節:
    fcw_ttc_incremental_approach.pdf
        11. Minimal Data Structure - Configuration
        (FPS / HISTORY_FRAMES / TTC_THRESHOLD / FORWARD_ROI)
    fcw_ttc_alert_decision_from_height_rate.pdf
        6. Basic Alert Threshold
        11. Choosing N (N = 5 と N = 10 を比較する)

実験ごとにパラメータを振るときは、このファイルだけを触れば済むようにしている。
"""

from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent

# SSDモデルと入出力のパス
MODEL = str(
    PROJECT_ROOT
    / "model"
    / "ssd_mobilenet_v2_fpnlite_320x320_coco17_tpu-8"
    / "saved_model"
)
VIDEO = str(PROJECT_ROOT / "mp4" / "sample06.mp4")
OUTPUT_DIR = PROJECT_ROOT / "output"

# 検出(Object detector)のパラメータ
CAR_CLASS_ID = 3
SCORE_THRESHOLD = 0.4

# Experiment 3 - Tracking のパラメータ
IOU_THRESHOLD = 0.3
MAX_MISSED_FRAMES = 150

# Experiment 1 - Basic TTC のパラメータ
# HISTORY_SIZEは保持するフレーム数、MIN_HISTORY_SIZEはTTC計算に必要な最小フレーム数
HISTORY_SIZE = 10
MIN_HISTORY_SIZE = 5

# Experiment 4 - Alert hysteresis のパラメータ
ON_TTC_THRESHOLD = 4.0
OFF_TTC_THRESHOLD = 6.5
R_SQUARED_THRESHOLD = 0.8
REQUIRED_COUNT = 3

# Experiment 2 - Forward ROI の台形の頂点(画像サイズに対する比率)
# (左上, 右上, 右下, 左下) の順で、道路の遠近に合わせて上辺を狭くしている
ROI_RATIOS = (
    (0.42, 0.45),
    (0.58, 0.45),
    (0.70, 0.95),
    (0.30, 0.95),
)
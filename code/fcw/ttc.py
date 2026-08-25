"""ボックス高さ履歴からTTCとアラートを求めるコンポーネント。

対応する資料の節:
    fcw_ttc_incremental_approach.pdf
        2. Experiment 1 - Basic TTC from Bounding-Box Height
        2.1 dh/dt = [h(t) - h(t - dt)] / dt
        2.2 TTCbasic = h / (dh/dt)
        3. Do Not Use Only Two Frames (5〜10フレームの履歴を使う)
        4. Only Calculate TTC for an Approaching Vehicle (dh/dt <= 0 なら警報なし)
        5. Basic Alert Logic (単一閾値)
    fcw_ttc_alert_decision_from_height_rate.pdf
        2.〜4. N-Frame Height History と毎フレームの再計算
        5. First Condition - Vehicle Must Be Approaching
        9. Complete First-Stage Decision Logic

        実装済み
        ・dequeによるN-frame Height History
        ・Linear Regressionによるdh/dt
        ・TTC計算
        ・単一ThresholdによるAlert

        未実装
        ・連続N回判定
        ・Hysteresis
        ・その他のExperiment 4 refinement
"""

from collections import deque

import numpy as np


def calculate_ttc(height_history, fps, min_history_size):
    """複数フレームのボックス高さ履歴から基本TTCを計算する。

    戻り値は (dh_dt, ttc)。履歴が足りない場合は (None, None)、
    接近していない場合は (dh_dt, None) を返す。
    """
    # 2フレームだけで判断すると検出枠のブレをそのまま拾ってしまうため、
    # 一定数の履歴が溜まるまではTTCを出さない(資料3.)
    if len(height_history) < min_history_size:
        return None, None

    if fps <= 0:
        return None, None

    frame_numbers = np.array(
        [frame for frame, _ in height_history],
        dtype=np.float64,
    )

    times = (frame_numbers - frame_numbers[0]) / fps

    heights = np.array(
        [height for _, height in height_history],
        dtype=np.float64,
    )

    slope, _ = np.polyfit(
        times,
        heights,
        1,
    )

    dh_dt = float(slope)

    if dh_dt <= 0:
        return dh_dt, None

    current_height = heights[-1]
    ttc = current_height / dh_dt

    return dh_dt, ttc

class TtcEstimator:
    """track_idごとに高さ履歴を保持し、毎フレームTTCを再計算する。"""

    def __init__(
        self,
        fps,
        history_size,
        min_history_size,
        ttc_threshold,
    ):
        self.fps = fps
        # 保持する履歴の最大フレーム数(N)。大きいほど滑らかだが反応は遅くなる
        self.history_size = history_size
        # TTCの計算を始めるのに必要な最小フレーム数
        self.min_history_size = min_history_size
        self.ttc_threshold = ttc_threshold
        # track_id -> [(frame_index, height_px), ...] の高さ履歴
        self.height_histories = {}

    def update(self, track_id, frame_index, height_px):
        """1台分の高さを履歴へ追加し、TTCとアラート判定の結果を返す。

        戻り値の辞書のキー
            dh_dt          : 高さの変化率 [px/s] (履歴不足ならNone)
            ttc            : TTCbasic [s] (接近していない/履歴不足ならNone)
            alert          : TTCが閾値未満かどうか
            history_length : 現在の履歴フレーム数
        """
        # 初めて見るIDなら空の履歴を作り、そこへ今回の高さを追加する
        history = self.height_histories.setdefault(
            track_id,
            deque(maxlen=self.history_size),
        )
        history.append((frame_index, height_px))

        dh_dt, ttc = calculate_ttc(
            history,
            self.fps,
            self.min_history_size,
        )

        # ttcがNone(履歴不足・非接近)のときは警報を出さない
        return {
            "dh_dt": dh_dt,
            "ttc": ttc,
            "alert": ttc is not None and ttc < self.ttc_threshold,
            "history_length": len(history),
        }

    def drop(self, track_id):
        """追跡が切れた車両の高さ履歴を破棄する。

        残したままにすると、履歴が際限なく増えるうえ、
        同じIDが再利用された場合に別の車の高さが混ざってしまう。
        """
        self.height_histories.pop(track_id, None)

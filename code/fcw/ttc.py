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

Experiment 4 (TTC refinement) で追加予定の線形回帰による傾き推定、
連続N回の閾値超え判定、ヒステリシス(ON: 2.0秒 / OFF: 2.5秒)は、
このコンポーネント内で完結するように分離してある。
"""


def calculate_ttc(height_history, fps, min_history_size):
    """複数フレームのボックス高さ履歴から基本TTCを計算する。

    戻り値は (dh_dt, ttc)。履歴が足りない場合は (None, None)、
    接近していない場合は (dh_dt, None) を返す。
    """
    # 2フレームだけで判断すると検出枠のブレをそのまま拾ってしまうため、
    # 一定数の履歴が溜まるまではTTCを出さない(資料3.)
    if len(height_history) < min_history_size:
        return None, None

    # 履歴の最古と最新の2点を結んだ傾きを、高さの変化率とみなす
    old_frame, old_height = height_history[0]
    current_frame, current_height = height_history[-1]

    # 経過フレームやfpsが不正だと時間に変換できない(ゼロ除算防止)
    delta_frames = current_frame - old_frame
    if delta_frames <= 0 or fps <= 0:
        return None, None

    # フレーム差を秒に換算してから、dh/dt [px/s] を求める
    delta_time = delta_frames / fps
    dh_dt = (current_height - old_height) / delta_time

    # 高さが増えていない車両は、接近していないものとしてTTCを出さない。
    # (資料上はTTC = 無限大に相当する。dh_dtは記録用に返す)
    if dh_dt <= 0:
        return dh_dt, None

    # TTCbasic = h / (dh/dt)。高さは距離にほぼ反比例するという近似に基づく
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
        history = self.height_histories.setdefault(track_id, [])
        history.append((frame_index, height_px))

        # 古いフレームから捨てて、直近history_size件だけを残す
        # (毎フレーム、窓を1つずらしながらTTCを計算し直すことになる)
        if len(history) > self.history_size:
            del history[:-self.history_size]

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

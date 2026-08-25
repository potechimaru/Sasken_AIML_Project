"""TTCにヒステリシスを適用する警報判定コンポーネント。

対応する資料の節:
    fcw_ttc_alert_decision_from_height_rate.pdf
        8. Hysteresis - Keep the Alert Stable
        9. Complete First-Stage Decision Logic

警報開始と解除に異なるTTC閾値を使い、閾値付近で警報が頻繁に
ON/OFFすることを防ぐ。状態はExperiment 3のtrack_idごとに保持する。
"""


class AlertDecision:
    """車両ごとのTTC警報状態をヒステリシス付きで管理する。"""

    def __init__(
        self,
        on_threshold=5.0,
        off_threshold=8.5,
        r_squared_threshold=0.8,
    ):
        if on_threshold >= off_threshold:
            raise ValueError(
                "on_threshold must be smaller than off_threshold"
            )

        self.on_threshold = on_threshold
        self.off_threshold = off_threshold
        self.r_squared_threshold = r_squared_threshold
        self.alert_status = {}

    def update(self, ttc, r_squared, track_id):
        """TTCから指定車両の警報状態を更新して返す。"""
        alert = self.alert_status.setdefault(track_id, False)

        # 履歴不足または非接近では、接近警報を解除する。
        if (ttc is None
            or r_squared is None
            or r_squared < self.r_squared_threshold
        ):
            alert = False
        elif not alert and ttc < self.on_threshold:
            alert = True
        elif alert and ttc > self.off_threshold:
            alert = False

        self.alert_status[track_id] = alert
        return alert

    def drop(self, track_id):
        """追跡が終了した車両の警報状態を破棄する。"""
        self.alert_status.pop(track_id, None)
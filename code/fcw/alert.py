"""TTCにヒステリシスを適用する警報判定コンポーネント。

対応する資料の節:
    fcw_ttc_alert_decision_from_height_rate.pdf
        8. Hysteresis - Keep the Alert Stable
        9. Complete First-Stage Decision Logic

警報開始と解除に異なるTTC閾値を使い、閾値付近で警報が頻繁に
ON/OFFすることを防ぐ。危険条件が連続Nフレーム続いたときだけ
警報を開始する。状態はExperiment 3のtrack_idごとに保持する。
"""


class AlertDecision:
    """車両ごとのTTC警報状態をヒステリシス付きで管理する。"""
    def __init__(
        self,
        on_threshold=5.0,
        off_threshold=8.5,
        required_count=3,
    ):
        if on_threshold >= off_threshold:
            raise ValueError(
                "on_threshold must be smaller than off_threshold"
            )
        if required_count < 1:
            raise ValueError("required_count must be at least 1")

        self.on_threshold = on_threshold
        self.off_threshold = off_threshold
        self.required_count = required_count
        self.alert_status = {}
        self.counter = {}

    def update(self, ttc, track_id):
        """TTCから指定車両の警報状態を更新して返す。"""
        alert = self.alert_status.setdefault(track_id, False)
        count = self.counter.setdefault(track_id, 0)

        # 警報ON中は、TTCなしまたはOFF閾値以上で解除する。
        if alert:
            if ttc is None or ttc >= self.off_threshold:
                alert = False
                count = 0
            else:
                alert = True

        # 警報OFF中は、危険条件が連続した場合だけONにする。
        else:
            if ttc is not None and ttc <= self.on_threshold:
                count += 1
                alert = count >= self.required_count
            else:
                count = 0
                alert = False

        self.alert_status[track_id] = alert
        self.counter[track_id] = count
        return alert

    def drop(self, track_id):
        """追跡が終了した車両の警報状態を破棄する。"""
        self.alert_status.pop(track_id, None)
        self.counter.pop(track_id, None)

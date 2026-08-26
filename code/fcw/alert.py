"""TTCにヒステリシスを適用する警報判定コンポーネント。

対応する資料の節:
    fcw_ttc_alert_decision_from_height_rate.pdf
        8. Hysteresis - Keep the Alert Stable
        9. Complete First-Stage Decision Logic

警報開始と解除に異なるTTC閾値を使い、閾値付近で警報が頻繁に
ON/OFFすることを防ぐ。危険条件が連続Nフレーム続いたときだけ
警報を開始する。線形回帰の決定係数が閾値未満の場合は
信頼できないTTCとして警報を解除する。状態はExperiment 3のtrack_idごとに保持する。
"""


class AlertDecision:
    """車両ごとのTTC警報状態をヒステリシス付きで管理する。"""
    def __init__(
        self,
        on_threshold=5.0,
        off_threshold=8.5,
        r_squared_threshold=0.8,
        required_count=3,
    ):
        if on_threshold >= off_threshold:
            raise ValueError(
                "on_threshold must be smaller than off_threshold"
            )
        if not 0.0 <= r_squared_threshold <= 1.0:
            raise ValueError(
                "r_squared_threshold must be between 0 and 1"
            )
        if required_count < 1:
            raise ValueError("required_count must be at least 1")

        self.on_threshold = on_threshold
        self.off_threshold = off_threshold
        self.r_squared_threshold = r_squared_threshold
        self.required_count = required_count
        self.alert_status = {}
        self.counter = {}

    def update(self, ttc, track_id, r_squared=1.0):
        """TTCと決定係数から指定車両の警報状態を更新して返す。

        r_squaredの既定値は、既存の呼び出し元との互換性を保つため1.0とする。
        """
        alert = self.alert_status.setdefault(track_id, False)
        count = self.counter.setdefault(track_id, 0)

        # 履歴不足、非接近、または回帰品質が低い場合は警報を解除する。
        # if (
        #     ttc is None
        #     or r_squared is None
        #     or r_squared < self.r_squared_threshold
        # ):
        #     alert = False
        #     count = 0
        # elif alert:
        #     # 警報ON中はOFF閾値を超えたら解除する(ヒステリシス)。
        #     if ttc > self.off_threshold:
        #         alert = False
        #         count = 0
        #     else:
        #         alert = True
        # else:
        #     # 警報OFF中は危険条件が連続した場合だけONにする。
        #     if ttc < self.on_threshold:
        #         count += 1
        #     else:
        #         count = 0
        #     #alert = count >= self.required_count

        #alert がon
        if alert is True:
            if ttc is not None and ttc >= self.off_threshold:
                alert = False
                count = 0
            else:
                alert = True
        #alert がoff
        else:
            if ttc is not None and ttc <= self.on_threshold:
                count += 1
                if count>= self.required_count:
                    alert = True
                else:
                    alert = False
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
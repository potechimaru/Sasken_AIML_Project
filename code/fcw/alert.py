class AlertDecision:
    def __init__(
            self, on_threshold, off_threshold
    ):
            self.on_threshold = on_threshold
            self.off_threshold = off_threshold

            self.alert_status = {}

    def update(self, ttc, track_id):
        alert = self.alert_status.setdefault(
             track_id,
             False,
        )

        if ttc is None:
            return alert
        
        if not alert:
             if ttc < self.on_threshold:
                 alert = True
        else:
             if ttc > self.off_threshold:
                  alert = False

        self.alert_status[track_id] = alert
        return alert

    def drop(self, track_id):
        self.alert_status.pop(track_id, None)
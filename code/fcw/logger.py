"""実験途中値をCSVへ保存するロガー。

対応する資料の節:
    fcw_ttc_alert_decision_from_height_rate.pdf
        12. Log the Intermediate Values (dh/dtやTTCなど途中値も残す)
        13. What to Plot

毎フレーム・車両ごとの途中値を溜め、終了時にCSVへ書き出す。
プロット自体は別スクリプトで行い、ここでは記録だけを担当する。
"""

from pathlib import Path
import time

import pandas as pd


class ExperimentLogger:
    """前方車両のTTC関連値をCSVへ書き出す。"""

    COLUMNS = (
        "frame",
        "time_s",
        "track_id",
        "height_px",
        "dh_dt",
        "ttc",
        "r_squared",
        "history_length",
        "alert",
        "score",
        "iou",
    )

    def __init__(self, log_dir: Path, fps: float):
        if fps <= 0:
            raise ValueError("fps must be positive")

        self.log_dir = Path(log_dir)
        self.fps = float(fps)
        self.rows = []
        self._closed = False

        self.log_dir.mkdir(parents=True, exist_ok=True)
        self.csv_path = (
            self.log_dir / f"ssd_ttc_log_{time.time():.0f}.csv"
        )

    def record(self, frame_index: int, car):
        """1台分の途中値をバッファへ追加する。"""
        if self._closed:
            raise RuntimeError("ExperimentLogger is already closed")

        self.rows.append(
            {
                "frame": frame_index,
                "time_s": (frame_index - 1) / self.fps,
                "track_id": car["track_id"],
                "height_px": car.get("height_px"),
                "dh_dt": car.get("dh_dt"),
                "ttc": car.get("ttc"),
                "r_squared": car.get("r_squared"),
                "history_length": car.get("history_length"),
                "alert": bool(car.get("alert", False)),
                "score": float(car["score"]) if "score" in car else None,
                "iou": car.get("iou"),
            }
        )

    def close(self):
        """バッファをCSVへ書き出して閉じる。"""
        if self._closed:
            return

        self._closed = True
        if not self.rows:
            print("experiment log: no rows to save")
            return

        df = pd.DataFrame(self.rows, columns=self.COLUMNS)
        df.to_csv(self.csv_path, index=False)
        print(f"saved experiment log: {self.csv_path}")
        print(f"rows: {len(self.rows)}")

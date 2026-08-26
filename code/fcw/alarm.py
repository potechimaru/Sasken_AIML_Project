"""FCW警報状態に応じてmacOSのシステム音を再生するコンポーネント。"""

import subprocess
import sys


class AlarmController:
    """macOSではafplayを制御し、他OSでは安全に何もしない。"""

    def __init__(
        self,
        sound_path="/System/Library/Sounds/Funk.aiff",
    ):
        self.sound_path = sound_path
        self.process = None

    @property
    def supported(self):
        """現在のOSで警報音を再生できるかを返す。"""
        return sys.platform == "darwin"

    def set_active(self, active):
        """警報状態に合わせて音声の再生・停止を切り替える。"""
        if active:
            self.start()
        else:
            self.stop()

    def start(self):
        """警報中にシステム音を繰り返し再生する。"""
        if not self.supported:
            return

        # 再生中なら何もしない。再生が完了していれば次の1回を開始する。
        if self.process is not None and self.process.poll() is None:
            return

        self.process = subprocess.Popen(
            ["afplay", self.sound_path],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

    def stop(self):
        """再生中の警報音を停止する。"""
        if self.process is None:
            return

        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=1.0)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()

        self.process = None

    def close(self):
        """プログラム終了時に警報音を確実に停止する。"""
        self.stop()
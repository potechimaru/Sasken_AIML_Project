import subprocess

class AlarmController:
    def __init__(self):
        self.process = None

    def start(self):
        """警報音を開始する"""

        # すでに警報音が鳴っている場合は何もしない
        if self.process is not None:
            return

        self.process = subprocess.Popen(
            [
                "bash",
                "-c",
                "while true; do afplay /System/Library/Sounds/Funk.aiff; done",
            ]
        )

    def stop(self):
        """警報音を停止する"""

        # 警報音が鳴っていなければ何もしない
        if self.process is None:
            return

        self.process.terminate()
        self.process.wait()

        self.process = None

    def close(self):
        """プログラム終了時に警報音を確実に停止する"""

        self.stop()
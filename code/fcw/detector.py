"""SSDによる車両検出コンポーネント。

対応する資料の節:
    fcw_ttc_incremental_approach.pdf
        2. Experiment 1 - "Detector output" (bbox = (x, y, width, height))
        7./10. Processing Flow の "Object detector" 段
    fcw_ttc_alert_decision_from_height_rate.pdf
        1. Basic Idea - 物体検出器が前方車のバウンディングボックスを与える前提

このコンポーネントは「検出まで」を担当し、ROI判定・追跡・TTC計算は行わない。
検出器を別モデル(YOLO等)へ差し替える場合も、detect()の戻り値の形を保てば
後段のコンポーネントは変更不要。
"""

import time

import cv2
import tensorflow as tf


class CarDetector:
    """SavedModel形式のSSDで、1フレームから車両検出結果を取り出す。"""

    def __init__(self, model_path, class_id, score_threshold):
        # SavedModelの読み込みは重いので、インスタンス生成時に1回だけ行う
        self.detect_fn = tf.saved_model.load(model_path)
        # class_idは検出対象のCOCOクラス(car=3)、score_thresholdは信頼度の下限
        self.class_id = class_id
        self.score_threshold = score_threshold

    def detect(self, frame_bgr):
        """1フレームを推論し、(車両リスト, 推論時間ms) を返す。

        車両リストの各要素は以下のキーを持つ辞書。
            box   : [ymin, xmin, ymax, xmax] の0〜1正規化座標
            score : 信頼度
            class : COCOクラスID
        """
        # OpenCVはBGR、SSDはRGBを前提とするので並びを入れ替える
        rgb_frame = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2RGB)
        x = tf.convert_to_tensor(rgb_frame, dtype=tf.uint8)
        # モデルはバッチ入力を期待するので、先頭に次元を足して
        # [batch_size, height, width, channels] の形にする
        x = x[tf.newaxis, ...]

        # 推論時間を計測する(リアルタイム処理が間に合うかの評価に使う)
        t0 = time.perf_counter()
        out = self.detect_fn(x)
        elapsed_ms = (time.perf_counter() - t0) * 1000.0

        # 出力は固定長の配列で返るため、有効な検出数numまでを切り出す。
        # バッチサイズ1で動かしているので、先頭の[0]が今のフレームの結果
        num = int(out["num_detections"][0])
        boxes = out["detection_boxes"][0][:num].numpy()
        scores = out["detection_scores"][0][:num].numpy()
        classes = out["detection_classes"][0][:num].numpy().astype(int)

        # 人やバイクなど車以外のクラスと、信頼度が低い検出をここで捨てる
        cars = []
        for i in range(num):
            if (
                classes[i] == self.class_id
                and scores[i] >= self.score_threshold
            ):
                cars.append({
                    "box": boxes[i],
                    "score": scores[i],
                    "class": classes[i],
                })

        return cars, elapsed_ms

# 実験１～３のベースコード

import time
import cv2
import tensorflow as tf
import numpy as np
from pathlib import Path

# SSDモデルと動画のパスを指定
PROJECT_ROOT = Path(__file__).resolve().parent.parent
MODEL = str(PROJECT_ROOT / "model" / "ssd_mobilenet_v2_fpnlite_320x320_coco17_tpu-8" / "saved_model")
VIDEO = str(PROJECT_ROOT / "mp4" / "sample02.mp4")

CAR_CLASS_ID = 3
SCORE_THRESHOLD = 0.4

MAX_MISSED_FRAMES = 5

# track_idごとの高さ履歴
HISTORY_SIZE = 10
MIN_HISTORY_SIZE = 5
TTC_THRESHOLD = 2.0

# IoUの閾値
IOU_THRESHOLD = 0.3

def calculate_iou(box1, box2):
    """2つのボックスのIoUを計算する。"""

    ymin1, xmin1, ymax1, xmax1 = box1
    ymin2, xmin2, ymax2, xmax2 = box2

    # 重なった領域の座標
    intersection_ymin = max(ymin1, ymin2)
    intersection_xmin = max(xmin1, xmin2)
    intersection_ymax = min(ymax1, ymax2)
    intersection_xmax = min(xmax1, xmax2)

    # 重なった領域の幅と高さ
    intersection_width = max(
        0.0, intersection_xmax - intersection_xmin
    )
    intersection_height = max(
        0.0, intersection_ymax - intersection_ymin
    )

    # 重なった領域の面積
    intersection_area = intersection_width * intersection_height

    # それぞれのボックスの面積
    box1_area = (xmax1 - xmin1) * (ymax1 - ymin1)
    box2_area = (xmax2 - xmin2) * (ymax2 - ymin2)

    # 2つのボックスを合わせた面積
    union_area = box1_area + box2_area - intersection_area

    if union_area <= 0:
        return 0.0

    return float(intersection_area / union_area)


def calculate_ttc(height_history, fps, min_history_size):
    """複数フレームのボックス高さ履歴から基本TTCを計算する。"""
    if len(height_history) < min_history_size:
        return None, None

    old_frame, old_height = height_history[0]
    current_frame, current_height = height_history[-1]

    delta_frames = current_frame - old_frame
    if delta_frames <= 0 or fps <= 0:
        return None, None

    delta_time = delta_frames / fps
    dh_dt = (current_height - old_height) / delta_time

    # 高さが増えていない車両は、接近していないものとしてTTCを出さない
    if dh_dt <= 0:
        return dh_dt, None

    ttc = current_height / dh_dt
    return dh_dt, ttc


def main():

    # SSDモデルを読み込む
    detect_fn = tf.saved_model.load(MODEL)
    print("loaded")


    capture = cv2.VideoCapture(VIDEO)

    # 動画出力ファイルのパス
    OUTPUT = str(PROJECT_ROOT / "output" / f"ttc_test3_result_{SCORE_THRESHOLD}_{time.time()}.mp4")

    frame_index = 0
    infer_ms_list = []

    # 動画のフレームレートとサイズを取得
    fps = capture.get(cv2.CAP_PROP_FPS) or 30.0
    width = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
    print(f"input: {width}x{height}, {fps:.2f} fps")

    # 動画出力ファイルを作成
    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    writer = cv2.VideoWriter(OUTPUT, fourcc, fps, (width, height))

    active_tracks = {}
    next_track_id = 1

    # track_idごとの高さ履歴
    height_histories = {}


    while True:
        ok, frame = capture.read()
        if not ok:
            break

        frame_index += 1

        # BGRからRGBに変換
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        # RGBをテンソルに変換
        x = tf.convert_to_tensor(rgb_frame, dtype=tf.uint8)
        # テンソルを[batch_size, height, width, channels]の形に変換
        x = x[tf.newaxis, ...]

        # 推論時間を計測
        t0 = time.perf_counter()

        # モデルに入力
        out = detect_fn(x)

        # 推論時間を計測
        elapsed_ms = (time.perf_counter() - t0) * 1000.0
        inference_fps = 1000.0 / elapsed_ms
        print(f"SSD inference FPS: {inference_fps:.2f}")

        print(f"______________frame={frame_index}______________")

        # print("出力パラメータ:")
        # print("  detection_boxes   : [ymin, xmin, ymax, xmax]  (0〜1 の正規化座標)")
        # print("  detection_scores  : 信頼度")
        # print("  detection_classes : COCOクラスID (car=3)")
        # print("  num_detections    : 有効な検出数")
        # for key, value in out.items():
        #     print(f"  {key}: shape={tuple(value.shape)}")

        # 推論時間をリストに追加
        infer_ms_list.append(elapsed_ms)
        
        # 推論時間をログ
        print(f"\ninference time: {elapsed_ms:.2f} ms\n")

        # 検出結果を表示
        # print("out:", out)

        # 検出結果を取り出す
        num = int(out["num_detections"][0])
        # print("num:", num)
        boxes = out["detection_boxes"][0][:num].numpy()
        # print("boxes:", boxes[:10])
        scores = out["detection_scores"][0][:num].numpy()
        # print("scores:", scores[:10])
        classes = out["detection_classes"][0][:num].numpy().astype(int)
        # print("classes:", classes[:10])

        # 車の検出結果を取り出す
        cars = []
        for i in range(num):
            if classes[i] == CAR_CLASS_ID and scores[i] >= SCORE_THRESHOLD:
                cars.append({
                    "box": boxes[i],
                    "score": scores[i],
                    "class": classes[i],
                })

        # 検出された車の数を表示
        print(f"cars={len(cars)}")

        # 画像の高さと幅を取得
        h, w = frame.shape[:2]
        # 画像をコピー
        vis = frame.copy()

        # 自車の進行方向を表す台形ROI
        roi_polygon = np.array(
            [
                (int(w * 0.42), int(h * 0.45)),  # 左上
                (int(w * 0.58), int(h * 0.45)),  # 右上
                (int(w * 0.70), int(h * 0.95)),  # 右下
                (int(w * 0.30), int(h * 0.95)),  # 左下
            ],
            dtype=np.int32,
        )

        cv2.polylines(
            vis,
            [roi_polygon],
            isClosed=True,
            color=(255, 0, 0),
            thickness=2,
        )

        # ROI内にいる車を保存する
        forward_cars = []

        for car in cars:
            ymin, xmin, ymax, xmax = car["box"]

            x1 = int(xmin * w)
            y1 = int(ymin * h)
            x2 = int(xmax * w)
            y2 = int(ymax * h)

            bottom_center_x = (x1 + x2) // 2
            bottom_center_y = y2

            inside_roi = cv2.pointPolygonTest(
                roi_polygon,
                (float(bottom_center_x), float(bottom_center_y)),
                False,
            ) >= 0

            if not inside_roi:
                continue

            # 後の追跡とTTCで使う情報を追加
            car["pixel_box"] = (x1, y1, x2, y2)
            car["height_px"] = float((ymax - ymin) * h)
            car["bottom_center"] = (bottom_center_x, bottom_center_y)

            forward_cars.append(car)

        print(f"all cars={len(cars)}")
        print(f"forward cars={len(forward_cars)}")

        # 既存トラックを一旦「このフレームでは未検出」として更新する
        for track in active_tracks.values():
            track["missed_frames"] += 1

        # このフレームより前から存在するトラックだけを対応候補にする
        candidate_track_ids = list(active_tracks.keys())
        matched_track_ids = set()

        for current_car in forward_cars:
            best_iou = 0.0
            best_track_id = None

            for track_id in candidate_track_ids:
                # 1つの既存トラックを複数車両へ割り当てない
                if track_id in matched_track_ids:
                    continue

                track = active_tracks[track_id]
                iou = calculate_iou(
                    track["box"],
                    current_car["box"],
                )

                if iou > best_iou:
                    best_iou = iou
                    best_track_id = track_id

            # 十分に重なっていれば、既存のtrack_idを引き継ぐ
            if (
                best_track_id is not None
                and best_iou >= IOU_THRESHOLD
            ):
                current_car["track_id"] = best_track_id
                matched_track_ids.add(best_track_id)

                # 最新の枠へ更新し、未検出回数をリセットする
                active_tracks[best_track_id]["box"] = (
                    current_car["box"].copy()
                )
                active_tracks[best_track_id]["missed_frames"] = 0
            else:
                # 対応相手がなければ、新しい車両としてIDを発行する
                track_id = next_track_id
                next_track_id += 1

                current_car["track_id"] = track_id
                active_tracks[track_id] = {
                    "box": current_car["box"].copy(),
                    "missed_frames": 0,
                }

            current_car["iou"] = best_iou

            # track_idごとに、フレーム番号とボックス高さを保存する
            track_id = current_car["track_id"]
            history = height_histories.setdefault(track_id, [])
            history.append((frame_index, current_car["height_px"]))
            if len(history) > HISTORY_SIZE:
                del history[:-HISTORY_SIZE]

            # 5～10フレームの高さ傾向からTTCを計算する
            dh_dt, ttc = calculate_ttc(
                history,
                fps,
                MIN_HISTORY_SIZE,
            )
            current_car["dh_dt"] = dh_dt
            current_car["ttc"] = ttc
            current_car["alert"] = (
                ttc is not None and ttc < TTC_THRESHOLD
            )

            if ttc is not None:
                print(
                    f"ID={track_id} "
                    f"height={current_car['height_px']:.1f}px "
                    f"dh/dt={dh_dt:.1f}px/s "
                    f"TTC={ttc:.2f}s "
                    f"alert={current_car['alert']}"
                )

        # 一定フレーム以上再検出できなかったトラックを削除する
        expired_track_ids = [
            track_id
            for track_id, track in active_tracks.items()
            if track["missed_frames"] > MAX_MISSED_FRAMES
        ]

        for track_id in expired_track_ids:
            del active_tracks[track_id]
            height_histories.pop(track_id, None)

        # 対応付け後の前方車両を描画する
        for car in forward_cars:
            x1, y1, x2, y2 = car["pixel_box"]
            bottom_center = car["bottom_center"]

            cv2.circle(
                vis,
                bottom_center,
                5,
                (0, 0, 255),
                -1,
            )

            box_color = (
                (0, 0, 255)
                if car["alert"]
                else (0, 255, 0)
            )

            cv2.rectangle(
                vis,
                (x1, y1),
                (x2, y2),
                box_color,
                2,
            )

            history_length = len(height_histories[car["track_id"]])
            label = (
                f"ID {car['track_id']} "
                f"car {float(car['score']):.2f} "
                f"h={car['height_px']:.1f} "
                f"IoU={car['iou']:.2f} "
                f"hist={history_length}"
            )
            cv2.putText(
                vis,
                label,
                (x1, max(0, y1 - 8)),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.6,
                box_color,
                2,
            )

            if history_length < MIN_HISTORY_SIZE:
                ttc_label = "TTC: calculating..."
            elif car["ttc"] is None:
                ttc_label = "TTC: N/A (not approaching)"
            else:
                ttc_label = f"TTC: {car['ttc']:.2f} s"
                if car["alert"]:
                    ttc_label += " ALERT"

            cv2.putText(
                vis,
                ttc_label,
                (x1, min(h - 8, y2 + 22)),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.6,
                box_color,
                2,
            )

        writer.write(vis)

        cv2.imshow("SSD Car detection", vis)
        if cv2.waitKey(1) & 0xFF == ord("q"):
            break
        

    writer.release()

    if infer_ms_list:
        # 初回推論は準備処理で遅いため除外
        steady_times = infer_ms_list[1:] or infer_ms_list
        mean_ms = sum(steady_times) / len(steady_times)
        average_fps = 1000.0 / mean_ms
        print("--- SSD inference result ---")
        print(f"frames: {len(infer_ms_list)}")
        print(f"mean inference time: {mean_ms:.2f} ms/frame")
        print(f"average SSD FPS: {average_fps:.2f}")

    capture.release()
    cv2.destroyAllWindows()
    print("done")

if __name__ == "__main__":
    main()
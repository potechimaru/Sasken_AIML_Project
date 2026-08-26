"""検出・ROI・TTCの結果を映像へ描画するコンポーネント。

対応する資料の節:
    fcw_ttc_incremental_approach.pdf
        13. Recommended First Milestone
        (TTCを映像に表示し、閾値を下回ったらアラートを出す)
    fcw_ttc_alert_decision_from_height_rate.pdf
        12. Log the Intermediate Values (dh/dtやTTCなど途中値も残す)
        13. What to Plot

判定ロジックはここには置かず、TtcEstimatorの結果を表示するだけにしている。
実験ごとに見せ方を変えたい場合はこのファイルだけを触る。
"""

import cv2

# OpenCVの色指定はBGR順。順に赤・緑・青になる
ALERT_COLOR = (0, 0, 255)
NORMAL_COLOR = (0, 255, 0)
ROI_COLOR = (255, 0, 0)


def draw_roi(image, roi_polygon):
    """前方ROIの台形を描画する。"""
    cv2.polylines(
        image,
        [roi_polygon],
        isClosed=True,
        color=ROI_COLOR,
        thickness=2,
    )


def draw_cars(image, cars, min_history_size):
    """前方車両の枠・下辺中心・TTCラベルを描画する。"""
    # 画面外へはみ出さないよう、ラベル位置のクリップに使う
    frame_height = image.shape[0]

    for car in cars:
        x1, y1, x2, y2 = car["pixel_box"]
        # 警報中の車だけ赤枠にして、ひと目で分かるようにする
        box_color = ALERT_COLOR if car["alert"] else NORMAL_COLOR

        # ROI判定に使った下辺中心の点(thickness=-1で塗りつぶし)
        cv2.circle(image, car["bottom_center"], 5, ALERT_COLOR, -1)
        cv2.rectangle(image, (x1, y1), (x2, y2), box_color, 2)

        # 枠の上側には、追跡と検出の途中値をまとめて出す
        label = (
            f"ID {car['track_id']} "
            f"car {float(car['score']):.2f} "
            f"h={car['height_px']:.1f} "
            f"IoU={car['iou']:.2f} "
            f"hist={car['history_length']}"
        )
        # 枠が画面上端に接しているときに文字が切れないよう、max(0, ...)で抑える
        cv2.putText(
            image,
            label,
            (x1, max(0, y1 - 8)),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.6,
            box_color,
            2,
        )

        # TTCは枠の下側に表示する(こちらも画面下端でクリップする)
        cv2.putText(
            image,
            _build_ttc_label(car, min_history_size),
            (x1, min(frame_height - 8, y2 + 22)),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.6,
            box_color,
            2,
        )


def _build_ttc_label(car, min_history_size):
    """履歴不足・非接近・TTC算出済みの3状態を文字列にする。"""
    # 履歴が溜まるまでの数フレームは計算中と表示する
    if car["history_length"] < min_history_size:
        return "TTC: calculating..."

    # 履歴は十分でもTTCがNoneなら、dh/dt <= 0 つまり接近していない状態
    if car["ttc"] is None:
        return "TTC: N/A (not approaching)"

    label = f"TTC: {car['ttc']:.2f} s"
    if car.get("r_squared") is not None:
        label += f" R2={car['r_squared']:.2f}"
    if car["alert"]:
        label += " ALERT"
    return label

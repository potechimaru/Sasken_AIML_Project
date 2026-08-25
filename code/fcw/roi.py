"""前方ROIによる検出結果の絞り込みコンポーネント。

対応する資料の節:
    fcw_ttc_incremental_approach.pdf
        8. Experiment 2 - Restrict Objects to the Forward Region
        8.1 Use the bottom-center of the bounding box
        8.2 ROI filtering logic
        9./10. Experiment 2 の入力と処理フロー

車線検出は行わず、画像上に固定した台形ROIで「明らかに前方でない物体」を落とすだけ。
自車線の厳密な判定が必要になった場合は、このコンポーネントを差し替える。
"""

import cv2
import numpy as np


def build_forward_roi(width, height, roi_ratios):
    """画像サイズと比率から、自車の進行方向を表す台形ROIを作る。

    比率で持っておくことで、解像度の違う動画でも同じ設定のまま使える。
    """
    # cv2.pointPolygonTestが整数座標の配列を要求するため、int32で作る
    return np.array(
        [
            (int(width * rx), int(height * ry))
            for rx, ry in roi_ratios
        ],
        dtype=np.int32,
    )


def filter_forward_cars(cars, roi_polygon, width, height):
    """バウンディングボックスの下辺中心がROI内にある車両だけを返す。

    ボックス中心ではなく下辺中心を使うのは、その点が路面との接地位置に
    ほぼ対応するため(資料8.1)。

    通過した車両には、後段の追跡・TTCで使う次のキーを追加する。
        pixel_box     : (x1, y1, x2, y2) のピクセル座標
        height_px     : ボックス高さ h(t) [px]
        bottom_center : 下辺中心 P = (x_center, y_bottom)
    """
    forward_cars = []

    for car in cars:
        # 検出結果は0〜1の正規化座標なので、画像サイズを掛けてピクセルに直す
        ymin, xmin, ymax, xmax = car["box"]

        x1 = int(xmin * width)
        y1 = int(ymin * height)
        x2 = int(xmax * width)
        y2 = int(ymax * height)

        # 下辺中心 P = (x_center, y_bottom)。y_bottomは枠の下端そのもの
        bottom_center_x = (x1 + x2) // 2
        bottom_center_y = y2

        # 第3引数のFalseは「距離ではなく内外の判定だけ返す」指定。
        # 戻り値は内側で+1、辺上で0、外側で-1になる
        inside_roi = cv2.pointPolygonTest(
            roi_polygon,
            (float(bottom_center_x), float(bottom_center_y)),
            False,
        ) >= 0

        # ROI外の車はこの先の追跡・TTC計算を一切行わない
        if not inside_roi:
            continue

        # 丸め誤差を避けるため、高さはピクセル化した値ではなく正規化座標から求める
        car["pixel_box"] = (x1, y1, x2, y2)
        car["height_px"] = float((ymax - ymin) * height)
        car["bottom_center"] = (bottom_center_x, bottom_center_y)

        forward_cars.append(car)

    return forward_cars

"""IoUベースの車両追跡コンポーネント。

対応する資料の節:
    fcw_ttc_incremental_approach.pdf
        1. Overall Incremental Development - Experiment 3 Tracking
        (object ID -> height history per vehicle)
        12. What Each Increment Adds - "3. Tracking"

高さ履歴が同一車両のものであることを保証するために、フレーム間で
track_idを引き継ぐ。見た目特徴を使う高度な追跡(資料の "Advanced appearance
tracking")は未導入で、前フレームの枠とのIoUだけで対応付けている。
"""


def calculate_iou(box1, box2):
    """2つのボックスのIoU(重なり面積 / 合計面積)を計算する。

    値は0.0〜1.0で、1.0に近いほど2つの枠がぴったり重なっている。
    """

    ymin1, xmin1, ymax1, xmax1 = box1
    ymin2, xmin2, ymax2, xmax2 = box2

    # 重なった領域の座標
    intersection_ymin = max(ymin1, ymin2)
    intersection_xmin = max(xmin1, xmin2)
    intersection_ymax = min(ymax1, ymax2)
    intersection_xmax = min(xmax1, xmax2)

    # 重なった領域の幅と高さ
    # 全く重なっていない場合は負になるので、0で打ち切る
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

    # 2つのボックスを合わせた面積(重なり分を二重に数えないよう引く)
    union_area = box1_area + box2_area - intersection_area

    # 面積0の不正な枠が来たときのゼロ除算対策
    if union_area <= 0:
        return 0.0

    return float(intersection_area / union_area)


class IouTracker:
    """フレーム間でtrack_idを維持する簡易トラッカー。"""

    def __init__(self, iou_threshold, max_missed_frames):
        # 同じ車とみなすIoUの下限
        self.iou_threshold = iou_threshold
        # 何フレーム続けて見失ったらそのIDを捨てるか(検出の一時的な抜けを許容する)
        self.max_missed_frames = max_missed_frames
        # track_id -> {"box": 直近の枠, "missed_frames": 連続で見失った回数}
        self.active_tracks = {}
        # 次に発行するID。使い回すと履歴が混ざるので、常に増やしていく
        self.next_track_id = 1

    def update(self, cars):
        """検出車両にtrack_idとiouを付与し、消滅したtrack_idの一覧を返す。

        引数のcarsは辞書のリストで、各要素に次のキーを追加する。
            track_id : 追跡ID
            iou      : 対応付けに使った最良IoU(新規車両は0.0)
        """
        # 既存トラックを一旦「このフレームでは未検出」として更新する。
        # このあと対応がついたものだけカウンタを0に戻すので、
        # 最後まで0に戻らなかったトラックが「見失った車」になる
        for track in self.active_tracks.values():
            track["missed_frames"] += 1

        # このフレームより前から存在するトラックだけを対応候補にする
        candidate_track_ids = list(self.active_tracks.keys())
        matched_track_ids = set()

        for current_car in cars:
            # この車と最もよく重なる既存トラックを総当たりで探す
            best_iou = 0.0
            best_track_id = None

            for track_id in candidate_track_ids:
                # 1つの既存トラックを複数車両へ割り当てない
                if track_id in matched_track_ids:
                    continue

                iou = calculate_iou(
                    self.active_tracks[track_id]["box"],
                    current_car["box"],
                )

                if iou > best_iou:
                    best_iou = iou
                    best_track_id = track_id

            # 十分に重なっていれば、既存のtrack_idを引き継ぐ
            if (
                best_track_id is not None
                and best_iou >= self.iou_threshold
            ):
                current_car["track_id"] = best_track_id
                matched_track_ids.add(best_track_id)

                # 次フレームの比較対象になるよう最新の枠へ更新し、
                # 未検出回数をリセットする。
                # 検出結果の配列を後から書き換えられないようcopyしておく
                self.active_tracks[best_track_id]["box"] = (
                    current_car["box"].copy()
                )
                self.active_tracks[best_track_id]["missed_frames"] = 0
            else:
                # 対応相手がなければ、新しい車両としてIDを発行する
                track_id = self.next_track_id
                self.next_track_id += 1

                current_car["track_id"] = track_id
                self.active_tracks[track_id] = {
                    "box": current_car["box"].copy(),
                    "missed_frames": 0,
                }

            current_car["iou"] = best_iou

        return self._remove_expired_tracks()

    def _remove_expired_tracks(self):
        """一定フレーム以上再検出できなかったトラックを削除する。"""
        expired_track_ids = [
            track_id
            for track_id, track in self.active_tracks.items()
            if track["missed_frames"] > self.max_missed_frames
        ]

        for track_id in expired_track_ids:
            del self.active_tracks[track_id]

        return expired_track_ids

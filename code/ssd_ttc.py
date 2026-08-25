# 分割前のコード
"""実験1〜3のベースコード(パイプライン統合のエントリポイント)。

各段の中身はfcwパッケージのコンポーネントに分かれている。
    Object detector      -> fcw/detector.py
    Experiment 2 ROI     -> fcw/roi.py
    Experiment 3 Tracking-> fcw/tracker.py
    Experiment 1 TTC     -> fcw/ttc.py
    可視化               -> fcw/visualizer.py
    設定値               -> fcw/config.py

このファイルは動画の読み書きと、資料の処理フロー
(detector -> ROI -> tracking -> TTC -> alert -> 描画)の接続だけを担当する。
"""

import time

import cv2

from fcw import config, roi, visualizer
from fcw.detector import CarDetector
from fcw.tracker import IouTracker
from fcw.ttc import TtcEstimator


def main():
    # --- 準備1: 検出器のロード ---
    # SavedModelの読み込みは数秒〜数十秒かかるため、ループの外で1回だけ行う
    detector = CarDetector(
        config.MODEL,
        config.CAR_CLASS_ID,
        config.SCORE_THRESHOLD,
    )
    print("loaded")

    # --- 準備2: 入力動画を開く ---
    capture = cv2.VideoCapture(config.VIDEO)

    # 動画のフレームレートとサイズを取得
    # fpsはdh/dtを求めるときに「フレーム差 -> 秒」へ換算するのに使う重要な値。
    # メタデータが壊れた動画では0が返るので、その場合は30fpsとみなす
    fps = capture.get(cv2.CAP_PROP_FPS) or 30.0
    width = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
    print(f"input: {width}x{height}, {fps:.2f} fps")

    # --- 準備3: 結果動画の出力先を用意する ---
    # 実験を繰り返しても上書きされないよう、閾値と時刻をファイル名に入れる
    config.OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_path = str(
        config.OUTPUT_DIR
        / f"ssd_ttc_result_{config.SCORE_THRESHOLD}_{time.time()}.mp4"
    )
    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    writer = cv2.VideoWriter(output_path, fourcc, fps, (width, height))

    # --- 準備4: 各コンポーネントを初期化する ---
    # ROIは画像サイズが変わらない限り不変なので、ここで1回だけ作る
    roi_polygon = roi.build_forward_roi(width, height, config.ROI_RATIOS)

    # トラッカーとTTC推定器はフレームをまたいで状態(ID・高さ履歴)を持つため、
    # ループの外で作り、毎フレーム同じインスタンスを使い続ける
    tracker = IouTracker(
        config.IOU_THRESHOLD,
        config.MAX_MISSED_FRAMES,
    )
    ttc_estimator = TtcEstimator(
        fps,
        config.HISTORY_SIZE,
        config.MIN_HISTORY_SIZE,
        config.TTC_THRESHOLD,
    )

    frame_index = 0
    infer_ms_list = []

    # --- メインループ: 1フレームずつ 検出 -> ROI -> 追跡 -> TTC -> 描画 ---
    while True:
        ok, frame = capture.read()
        # 動画の終端に達するとokがFalseになる
        if not ok:
            break

        # frame_indexは高さ履歴の時刻(何フレーム前か)を表すのに使う
        frame_index += 1

        # 手順1: SSDで車両を検出する(この時点ではROI外の車も含む)
        cars, elapsed_ms = detector.detect(frame)
        infer_ms_list.append(elapsed_ms)

        print(f"SSD inference FPS: {1000.0 / elapsed_ms:.2f}")
        print(f"______________frame={frame_index}______________")
        print(f"\ninference time: {elapsed_ms:.2f} ms\n")

        # 手順2 (Experiment 2): 下辺中心が前方ROI内にある車両だけを残す。
        # 対向車や歩道側の駐車車両など、自車の前方にいない車をここで除外する
        forward_cars = roi.filter_forward_cars(
            cars,
            roi_polygon,
            width,
            height,
        )
        print(f"all cars={len(cars)}")
        print(f"forward cars={len(forward_cars)}")

        # 手順3 (Experiment 3): 前フレームとの対応をとってtrack_idを引き継ぐ。
        # これで「同じ車の高さ履歴」が保証される。
        # 画面から消えた車のIDが返るので、そのIDの高さ履歴も一緒に破棄する
        expired_track_ids = tracker.update(forward_cars)
        for track_id in expired_track_ids:
            ttc_estimator.drop(track_id)

        # 手順4 (Experiment 1): 高さ履歴からdh/dtとTTCを求め、アラートを判定する
        for car in forward_cars:
            # 計算結果(dh_dt / ttc / alert / history_length)をcarへマージし、
            # 後続の描画処理が1つの辞書だけを見れば済むようにする
            car.update(
                ttc_estimator.update(
                    car["track_id"],
                    frame_index,
                    car["height_px"],
                )
            )

            # 履歴不足や非接近(dh/dt <= 0)の場合はTTCがNoneになるので、
            # 数値が出たものだけをログに残す
            if car["ttc"] is not None:
                print(
                    f"ID={car['track_id']} "
                    f"height={car['height_px']:.1f}px "
                    f"dh/dt={car['dh_dt']:.1f}px/s "
                    f"TTC={car['ttc']:.2f}s "
                    f"alert={car['alert']}"
                )

        # 手順5: 元のフレームを壊さないようコピーしてから描画する
        vis = frame.copy()
        visualizer.draw_roi(vis, roi_polygon)
        visualizer.draw_cars(vis, forward_cars, config.MIN_HISTORY_SIZE)

        # 手順6: 結果を動画ファイルへ保存しつつ、画面にも表示する
        writer.write(vis)

        cv2.imshow("SSD Car detection", vis)
        # qキーで途中終了できるようにする
        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    writer.release()

    # --- 後片付け: 推論速度の集計(リアルタイム処理が可能かの確認用) ---
    if infer_ms_list:
        # 初回推論は準備処理で遅いため除外
        steady_times = infer_ms_list[1:] or infer_ms_list
        mean_ms = sum(steady_times) / len(steady_times)
        print("--- SSD inference result ---")
        print(f"frames: {len(infer_ms_list)}")
        print(f"mean inference time: {mean_ms:.2f} ms/frame")
        print(f"average SSD FPS: {1000.0 / mean_ms:.2f}")

    capture.release()
    cv2.destroyAllWindows()
    print("done")


if __name__ == "__main__":
    main()

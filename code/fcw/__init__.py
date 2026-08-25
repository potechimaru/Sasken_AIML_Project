"""カメラ画像ベースの前方衝突警報(FCW)コンポーネント群。

各モジュールと配布資料(PDF)の実験(Experiments)の対応:

    config.py     : 両PDFのConfiguration節(FPS / HISTORY_FRAMES / TTC_THRESHOLD / FORWARD_ROI)
    detector.py   : 全実験共通の入力段 "Object detector"
    roi.py        : Experiment 2 - Forward ROI
    tracker.py    : Experiment 3 - Tracking
    ttc.py        : Experiment 1 - Basic TTC / Experiment 4 - TTC refinement
    visualizer.py : 実験結果の可視化(TTC alert decisionのログ・プロット節に相当)
"""

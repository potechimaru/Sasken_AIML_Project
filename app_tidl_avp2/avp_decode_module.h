#ifndef AVP_DECODE_MODULE_H_
#define AVP_DECODE_MODULE_H_

#include "avp_common.h"

/*
 * The decoder is intentionally opaque to app_tidl_avp2/main.c.  The first
 * call creates the GStreamer pipeline and subsequent calls return one NV12
 * frame from the same pipeline.
 */
typedef struct AvpDecodeContext AvpDecodeContext;

/*
 * avp_decode_h264_next_frame()が入力ストリームの終端に到達した場合の戻り値。
 */
#define AVP_DECODE_EOS ((vx_status)1)

/*
 * H.264エレメンタリーストリームから、次の1フレームをデコードする。
 *
 * context_ptr   : デコーダーコンテキストのアドレス。最初の呼び出しでは
 *                 NULLのコンテキストを渡し、2回目以降は同じコンテキストを渡す。
 * h264_path     : H.264ファイルのパス。
 * nv12_data     : デコードされたNV12データへのポインタを受け取る出力引数。
 *                 Yプレーンの後ろにUVプレーンが続くパック形式で返される。
 *                 ポインタの内容は次回呼び出し時に更新される可能性がある。
 * nv12_size     : 返されたNV12データのサイズを受け取る出力引数（バイト単位）。
 * width         : デコード画像の幅を受け取る出力引数（ピクセル単位）。
 * height        : デコード画像の高さを受け取る出力引数（ピクセル単位）。
 * repair_enable : 0の場合は修復を無効化、0以外の場合は破損フレームの
 *                 修復・補間を有効化する。
 *
 * 戻り値        : VX_SUCCESSならフレーム取得成功、AVP_DECODE_EOSなら終端、
 *                 その他の値ならデコードまたはバッファ処理の失敗。
 */
vx_status avp_decode_h264_next_frame(
    AvpDecodeContext **context,
    const char *h264_path,
    const uint8_t **nv12_data,
    size_t *nv12_size,
    uint32_t *width,
    uint32_t *height,
    vx_bool repair_enable);

/*
 * GStreamerパイプライン、デコーダーコンテキスト、内部NV12バッファを解放する。
 *
 * context_ptr : 解放対象のデコーダーコンテキストのアドレス。解放後はNULLになる。
 */
void avp_decode_release(AvpDecodeContext **context);

/*
 * デコード済みNV12フレームを修復または補間する。
 * current_validがtrueならcurrentをそのままコピーする。falseの場合、
 * previousとnextが両方あれば各バイトの平均値で補間し、片方だけならその
 * フレームを使用する。どちらもなければoutputを0で初期化する。
 *
 * output        : 修復・補間したNV12データを書き込む出力バッファ。
 * previous      : 直前の正常なNV12フレーム。利用できない場合はNULL。
 * current       : 現在のNV12フレーム。正常な場合に使用する。
 * next          : 直後の正常なNV12フレーム。利用できない場合はNULL。
 * frame_size    : 処理対象NV12フレームのサイズ（バイト単位）。
 * current_valid : currentが正常ならvx_true_e、破損していればvx_false_e。
 */
void avp_decode_repair_nv12_frame(
    uint8_t *output,
    const uint8_t *previous,
    const uint8_t *current,
    const uint8_t *next,
    size_t frame_size,
    vx_bool current_valid);

#endif /* AVP_DECODE_MODULE_H_ */

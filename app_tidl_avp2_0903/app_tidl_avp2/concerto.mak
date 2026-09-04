ifeq ($(TARGET_CPU),$(filter $(TARGET_CPU), x86_64 A72 A53))

include $(PRELUDE)

TARGET      := vx_app_tidl_avp2
CSOURCES    := main.c avp_decode_module.c avp_scaler_module.c avp_pre_proc_module.c avp_tidl_module.c avp_post_proc_module.c fisheye_angle_table.c avp_img_mosaic_module.c avp_draw_detections_module.c avp_display_module.c

# FCW/TTC core。sourceは複製せず、単一の正本を相対pathで参照する。
# このpathはSDK内の配置基準であり、app が vision_apps/apps/dl_demos/app_tidl_avp2/
# に置かれる場合、../../codeC は vision_apps/apps/codeC を指す。
CSOURCES    += ../../codeC/main_pre.c
CSOURCES    += ../../codeC/fcw_types.c
CSOURCES    += ../../codeC/fcw_tidl_adapter.c
CSOURCES    += ../../codeC/avp_fcw_roi.c
CSOURCES    += ../../codeC/fcw_tracker.c
CSOURCES    += ../../codeC/fcw_ttc.c
CSOURCES    += ../../codeC/fcw_alert.c
CSOURCES    += ../../codeC/fcw_alarm.c

# concertoはIDIRSの相対pathを解決できない（compilerのCWDがvision_apps/のため）。
# CSOURCESはmodule dir基準で展開されるが、IDIRSは絶対pathで指定する必要がある。
IDIRS       += $(VISION_APPS_PATH)/apps/codeC
IDIRS       += $(VISION_APPS_PATH)/apps/dl_demos/app_tidl_avp2

ifeq ($(HOST_COMPILER),GCC_LINUX)
CFLAGS += -Wno-unused-function
endif

ifeq ($(TARGET_CPU),x86_64)

TARGETTYPE  := exe

CSOURCES    += main_x86.c

include $(VISION_APPS_PATH)/apps/concerto_x86_64_inc.mak

IDIRS       += $(VISION_APPS_KERNELS_IDIRS)

STATIC_LIBS += $(VISION_APPS_KERNELS_LIBS)
STATIC_LIBS += $(TIADALG_LIBS)

endif

ifeq ($(TARGET_OS),$(filter $(TARGET_OS), LINUX QNX))
ifeq ($(TARGET_CPU),$(filter $(TARGET_CPU), A72 A53))

TARGETTYPE  := exe

CSOURCES    += main_linux_arm.c

include $(VISION_APPS_PATH)/apps/concerto_mpu_inc.mak

IDIRS       += $(VISION_APPS_KERNELS_IDIRS)

STATIC_LIBS += $(VISION_APPS_KERNELS_LIBS)

ifeq ($(TARGET_OS), LINUX)

# avp_decode_module.c は H.264 デコードに GStreamer を使用する
CFLAGS      += -DLINUX

ifneq ($(LINUX_FS_PATH),)
IDIRS       += $(LINUX_FS_PATH)/usr/include/gstreamer-1.0
IDIRS       += $(LINUX_FS_PATH)/usr/include/glib-2.0
IDIRS       += $(LINUX_FS_PATH)/usr/lib/glib-2.0/include
endif

SHARED_LIBS += gstreamer-1.0
SHARED_LIBS += gstapp-1.0
SHARED_LIBS += gstvideo-1.0
SHARED_LIBS += gstbase-1.0
SHARED_LIBS += gobject-2.0
SHARED_LIBS += glib-2.0

endif

endif
endif

IDIRS       += $(EDGEAI_IDIRS)
SHARED_LIBS += edgeai-apps-utils
SHARED_LIBS += edgeai-tiovx-kernels

ifeq ($(SOC),j722s)
SKIPBUILD=1
endif

include $(FINALE)

endif

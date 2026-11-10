// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ziptv2: the display entry points of the video_player_videohole wrapper
// (libftpw_video_player_videohole.so, copied from that package), used to
// render the video on the hardware video plane instead of a texture.

#ifndef FLUTTER_TIZEN_PLUGINS_WRAPPER_VIDEO_PLAYER_DISPLAY_H_
#define FLUTTER_TIZEN_PLUGINS_WRAPPER_VIDEO_PLAYER_DISPLAY_H_

typedef enum {
  PLAYER_DISPLAY_TYPE_TCORE_OVERLAY, /**< Video overlay using
                                        tizen_core_wl_window_h */
  PLAYER_DISPLAY_TYPE_TCORE_OVERLAY_SYNC_UI,
} player_display_type_tcore_e;

#ifdef __cplusplus
extern "C" {
#endif

int ftpw_video_player_videohole_player_set_ecore_wl_display(
    void *player, int type, void *ecore_wl_window, int x, int y, int width,
    int height);
int ftpw_video_player_videohole_player_set_tcore_display(
    void *player, player_display_type_tcore_e type, void *display);

#ifdef __cplusplus
}
#endif

#endif  // FLUTTER_TIZEN_PLUGINS_WRAPPER_VIDEO_PLAYER_DISPLAY_H_

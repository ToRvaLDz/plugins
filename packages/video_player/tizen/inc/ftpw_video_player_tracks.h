// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ziptv2: the track info entry points of the video_player_videohole wrapper
// (libftpw_video_player_videohole.so, copied from that package), used to read
// the codec and channel count of audio tracks and the codec and kind
// (text or picture) of subtitle tracks.

#ifndef FLUTTER_TIZEN_PLUGINS_WRAPPER_VIDEO_PLAYER_TRACKS_H_
#define FLUTTER_TIZEN_PLUGINS_WRAPPER_VIDEO_PLAYER_TRACKS_H_

#ifndef MMPLAYER_FOUR_CC_LEN
#define MMPLAYER_FOUR_CC_LEN 14
#endif
#ifndef PLAYER_LANG_NAME_SIZE
#define PLAYER_LANG_NAME_SIZE 10
#endif

typedef struct {
  char fourCC[MMPLAYER_FOUR_CC_LEN + 1]; /**< codec fourcc */
  char language[PLAYER_LANG_NAME_SIZE];  /**< language info*/
  int sample_rate;                       /**< sample rate in this track*/
  int channel;                           /**< channel in this track*/
  int bit_rate;                          /**< bitrate in this track*/
} player_audio_track_info_v2;

typedef struct {
  char fourCC[MMPLAYER_FOUR_CC_LEN + 1]; /**< codec fourcc */
  char language[PLAYER_LANG_NAME_SIZE];  /**< language info*/
  int subtitle_type; /**< text subtitle = 0, picture subtitle = 1 */
} player_subtitle_track_info_v2;

#ifdef __cplusplus
extern "C" {
#endif

int ftpw_video_player_videohole_player_get_track_count_v2(void *player,
                                                          int type,
                                                          int *pcount);
int ftpw_video_player_videohole_player_get_audio_track_info_v2(
    void *player, int index, player_audio_track_info_v2 **track_info);
int ftpw_video_player_videohole_player_get_subtitle_track_info_v2(
    void *player, int index, player_subtitle_track_info_v2 **track_info);

#ifdef __cplusplus
}
#endif

#endif  // FLUTTER_TIZEN_PLUGINS_WRAPPER_VIDEO_PLAYER_TRACKS_H_

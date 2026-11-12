// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_PLUGIN_VIDEO_PLAYER_H_
#define FLUTTER_PLUGIN_VIDEO_PLAYER_H_

#include <flutter/encodable_value.h>
#include <flutter/event_channel.h>
#include <flutter/plugin_registrar.h>
#include <flutter_tizen.h>
#include <flutter/texture_registrar.h>
#include <glib.h>
#include <player.h>

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "video_player_options.h"

namespace video_player_tizen {

struct AudioTrack {
  int32_t index;
  std::optional<std::string> language;
  bool is_selected;
  // ziptv2: codec fourCC and channel count from the v2 track info.
  std::optional<std::string> codec;
  std::optional<int32_t> channel_count;
};

class VideoPlayer {
 public:
  using SeekCompletedCallback = std::function<void()>;

  explicit VideoPlayer(flutter::PluginRegistrar *plugin_registrar,
                       flutter::TextureRegistrar *texture_registrar,
                       const std::string &uri, VideoPlayerOptions &options,
                       flutter::EncodableMap &http_headers);
  ~VideoPlayer();

  void Play();
  void Pause();
  void SetLooping(bool is_looping);
  void SetVolume(double volume);
  void SetPlaybackSpeed(double speed);
  void SeekTo(int32_t position, SeekCompletedCallback callback);
  // ziptv2: video hole. True when the video renders on the hardware video
  // plane (no texture); the area is set with SetDisplayRoi in window pixels.
  bool IsVideoHole() const { return video_hole_; }
  void SetDisplayRoi(int32_t x, int32_t y, int32_t width, int32_t height);
  // ziptv2: embedded text subtitles. GetTextTracks returns one map per track
  // (index, language, isSelected); SelectTextTrack(-1) turns them off. The
  // text arrives as "subtitle" events (text, duration in ms) while enabled.
  flutter::EncodableList GetTextTracks();
  void SelectTextTrack(int64_t index);
  // ziptv2: the raw registrar, needed to reach the native window.
  static void SetRegistrarRef(FlutterDesktopPluginRegistrarRef registrar_ref);
  int32_t GetPosition();
  std::vector<AudioTrack> GetAudioTracks();
  void SelectAudioTrack(int64_t index);
  void SetPreventsDisplaySleep(bool prevents_display_sleep);
  void Dispose();

  int64_t GetTextureId() { return texture_id_; }

 private:
  void SendPendingEvents();
  void ScheduleSendPendingEvents();
  void PushEvent(const flutter::EncodableValue &encodable_value);
  void SendError(const std::string &error_code,
                 const std::string &error_message);
  FlutterDesktopGpuSurfaceDescriptor *ObtainGpuSurface(size_t width,
                                                       size_t height);

  void SetUpEventChannel(flutter::BinaryMessenger *messenger);
  void Initialize();
  void SendInitialized();
  void SendIsPlayingStateUpdate(bool is_playing);

  static void OnPrepared(void *data);
  static void OnBuffering(int percent, void *data);
  static void OnSubtitleUpdated(unsigned long duration, char *text,
                                void *data);
  static void OnSeekCompleted(void *data);
  static void OnPlayCompleted(void *data);
  static void OnInterrupted(player_interrupted_code_e code, void *data);
  static void OnError(int error_code, void *data);
  static void OnVideoFrameDecoded(media_packet_h packet, void *data);
  static void ReleaseMediaPacket(void *packet);
#ifdef TV_PROFILE
  void StartScreensaverTimer();
  static gboolean ResetScreensaverTimeout(gpointer data);
#endif

  void RequestRendering();
  void OnRenderingCompleted();
  int64_t GetDuration();
  int64_t GetLiveDuration();
  bool IsLive();
  void SetStreamingBufferingTime(const std::string &value);
  void SetVideoHoleDisplay(const std::string &window_geometry);

  media_packet_h current_media_packet_ = nullptr;
  media_packet_h previous_media_packet_ = nullptr;

  bool is_initialized_ = false;
  bool is_rendering_ = false;
  bool is_live_ = false;
  // ziptv2: http source whose duration is reported as 1 ms (endless stream,
  // e.g. a raw MPEG-TS live). Unlike is_live_, it is never overwritten by the
  // adaptive streaming info, which reports "not live" for a raw TS on TV.
  bool is_endless_ = false;
  bool video_hole_ = false;
  bool subtitles_enabled_ = false;
  // ziptv2: streaming buffering time read back after it was set, in ms
  // (-1 = not requested or not readable).
  int buffering_prebuffer_ms_ = -1;
  int buffering_rebuffer_ms_ = -1;

  std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>>
      event_channel_;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> event_sink_;

  player_h player_ = nullptr;
  int64_t texture_id_ = -1;
  std::string uri_;

  flutter::TextureRegistrar *texture_registrar_;
  std::unique_ptr<flutter::TextureVariant> texture_variant_;
  std::unique_ptr<FlutterDesktopGpuSurfaceDescriptor> gpu_surface_;
  std::mutex mutex_;
  std::queue<media_packet_h> packet_queue_;

  SeekCompletedCallback on_seek_completed_;

#ifdef TV_PROFILE
  guint timer_id_ = 0;
  bool prevents_display_sleep_ = true;
#endif

  // Event dispatch state structure for lifecycle management
  struct EventDispatchState {
    std::mutex mutex;
    VideoPlayer *player = nullptr;
    bool disposed = false;
    guint pending_source_id = 0;
  };
  std::shared_ptr<EventDispatchState> event_dispatch_state_;

  struct GMainContextDeleter {
    void operator()(GMainContext *context) const {
      g_main_context_unref(context);
    }
  };
  std::unique_ptr<GMainContext, GMainContextDeleter> main_context_;

  std::mutex queue_mutex_;
  std::queue<flutter::EncodableValue> encodable_event_queue_;
  std::queue<std::pair<std::string, std::string>> error_event_queue_;
};

}  // namespace video_player_tizen

#endif  // FLUTTER_PLUGIN_VIDEO_PLAYER_H_

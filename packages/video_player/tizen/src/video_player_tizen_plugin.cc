// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "video_player_tizen_plugin.h"

#include <app_common.h>
#include <flutter/encodable_value.h>
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar.h>
#include <flutter/standard_method_codec.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "log.h"
#include "messages.h"
#include "video_player.h"
#include "video_player_error.h"
#include "video_player_options.h"

namespace video_player_tizen {

class VideoPlayerTizenPlugin : public flutter::Plugin,
                               public TizenVideoPlayerApi {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrar *registrar);

  VideoPlayerTizenPlugin(flutter::PluginRegistrar *registrar);
  virtual ~VideoPlayerTizenPlugin();

  std::optional<FlutterError> Initialize() override;
  ErrorOr<TextureMessage> Create(const CreateMessage &msg) override;
  std::optional<FlutterError> Dispose(const TextureMessage &msg) override;
  std::optional<FlutterError> SetLooping(const LoopingMessage &msg) override;
  std::optional<FlutterError> SetVolume(const VolumeMessage &msg) override;
  std::optional<FlutterError> SetPlaybackSpeed(
      const PlaybackSpeedMessage &msg) override;
  std::optional<FlutterError> Play(const TextureMessage &msg) override;
  ErrorOr<PositionMessage> Position(const TextureMessage &msg) override;
  void SeekTo(
      const PositionMessage &msg,
      std::function<void(std::optional<FlutterError> reply)> result) override;
  std::optional<FlutterError> Pause(const TextureMessage &msg) override;
  std::optional<FlutterError> SetMixWithOthers(
      const MixWithOthersMessage &msg) override;
  std::optional<FlutterError> SetPreventsDisplaySleep(
      const PreventsDisplaySleepMessage &msg) override;
  ErrorOr<flutter::EncodableList> GetAudioTracks(
      const TextureMessage &msg) override;
  std::optional<FlutterError> SelectAudioTrack(
      const SelectAudioTrackMessage &msg) override;

 private:
  void DisposeAllPlayers();

  flutter::PluginRegistrar *plugin_registrar_;
  flutter::TextureRegistrar *texture_registrar_;
  // ziptv2: "setDisplayRoi" for the video hole (see VideoPlayer::SetDisplayRoi).
  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>>
      video_hole_channel_;
  // ziptv2: "getTextTracks" / "selectTextTrack" for embedded subtitles.
  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>>
      tracks_channel_;
  VideoPlayerOptions options_;
  std::map<int64_t, std::unique_ptr<VideoPlayer>> players_;
};

void VideoPlayerTizenPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrar *registrar) {
  auto plugin = std::make_unique<VideoPlayerTizenPlugin>(registrar);
  registrar->AddPlugin(std::move(plugin));
}

VideoPlayerTizenPlugin::VideoPlayerTizenPlugin(
    flutter::PluginRegistrar *registrar)
    : plugin_registrar_(registrar) {
  texture_registrar_ = registrar->texture_registrar();

  TizenVideoPlayerApi::SetUp(registrar->messenger(), this);

  video_hole_channel_ =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "ziptv2/video_player_tizen/video_hole",
          &flutter::StandardMethodCodec::GetInstance());
  video_hole_channel_->SetMethodCallHandler(
      [this](const flutter::MethodCall<flutter::EncodableValue> &call,
             std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>>
                 result) {
        const auto *args =
            std::get_if<flutter::EncodableMap>(call.arguments());
        if (call.method_name() != "setDisplayRoi" || !args) {
          result->NotImplemented();
          return;
        }
        auto get_int = [args](const char *key) -> int64_t {
          auto iter = args->find(flutter::EncodableValue(key));
          if (iter == args->end()) return 0;
          if (const auto *v = std::get_if<int32_t>(&iter->second)) return *v;
          if (const auto *v = std::get_if<int64_t>(&iter->second)) return *v;
          return 0;
        };
        auto iter = players_.find(get_int("textureId"));
        if (iter != players_.end()) {
          iter->second->SetDisplayRoi(
              static_cast<int32_t>(get_int("x")),
              static_cast<int32_t>(get_int("y")),
              static_cast<int32_t>(get_int("width")),
              static_cast<int32_t>(get_int("height")));
        }
        result->Success();
      });

  tracks_channel_ =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "ziptv2/video_player_tizen/tracks",
          &flutter::StandardMethodCodec::GetInstance());
  tracks_channel_->SetMethodCallHandler(
      [this](const flutter::MethodCall<flutter::EncodableValue> &call,
             std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>>
                 result) {
        const auto *args =
            std::get_if<flutter::EncodableMap>(call.arguments());
        if (!args) {
          result->Error("Invalid argument", "Arguments must be a map.");
          return;
        }
        auto get_int = [args](const char *key, int64_t fallback) -> int64_t {
          auto iter = args->find(flutter::EncodableValue(key));
          if (iter == args->end()) return fallback;
          if (const auto *v = std::get_if<int32_t>(&iter->second)) return *v;
          if (const auto *v = std::get_if<int64_t>(&iter->second)) return *v;
          return fallback;
        };
        auto iter = players_.find(get_int("textureId", -1));
        if (iter == players_.end()) {
          result->Error("Invalid argument", "No player for this textureId.");
          return;
        }
        try {
          if (call.method_name() == "getTextTracks") {
            result->Success(
                flutter::EncodableValue(iter->second->GetTextTracks()));
          } else if (call.method_name() == "selectTextTrack") {
            iter->second->SelectTextTrack(get_int("index", -1));
            result->Success();
          } else {
            result->NotImplemented();
          }
        } catch (const VideoPlayerError &error) {
          result->Error(error.code(), error.message());
        }
      });
}

VideoPlayerTizenPlugin::~VideoPlayerTizenPlugin() { DisposeAllPlayers(); }

void VideoPlayerTizenPlugin::DisposeAllPlayers() {
  for (const auto &[id, player] : players_) {
    player->Dispose();
  }
  players_.clear();
}

std::optional<FlutterError> VideoPlayerTizenPlugin::Initialize() {
  DisposeAllPlayers();
  return std::nullopt;
}

ErrorOr<TextureMessage> VideoPlayerTizenPlugin::Create(
    const CreateMessage &msg) {
  std::string uri;
  flutter::EncodableMap http_headers = {};

  if (msg.asset() && !msg.asset()->empty()) {
    char *res_path = app_get_resource_path();
    if (res_path) {
      uri = uri + res_path + "flutter_assets/" + *msg.asset();
      free(res_path);
    } else {
      return FlutterError("Internal error", "Failed to get resource path.");
    }
  } else if (msg.uri() && !msg.uri()->empty()) {
    uri = *msg.uri();

    const flutter::EncodableMap &http_headers_map = msg.http_headers();
    if (!http_headers_map.empty()) {
      http_headers = http_headers_map;
    }
  } else {
    return FlutterError("Invalid argument", "Either asset or uri must be set.");
  }
  LOG_DEBUG("[VideoPlayerTizenPlugin] uri: %s", uri.c_str());

  int64_t texture_id = 0;
  try {
    auto player = std::make_unique<VideoPlayer>(
        plugin_registrar_, texture_registrar_, uri, options_, http_headers);
    texture_id = player->GetTextureId();
    players_[texture_id] = std::move(player);
  } catch (const VideoPlayerError &error) {
    return FlutterError(error.code(), error.message());
  }

  TextureMessage result(texture_id);
  return result;
}

std::optional<FlutterError> VideoPlayerTizenPlugin::Dispose(
    const TextureMessage &msg) {
  auto iter = players_.find(msg.texture_id());
  if (iter != players_.end()) {
    iter->second->Dispose();
    players_.erase(iter);
  }
  return std::nullopt;
}

std::optional<FlutterError> VideoPlayerTizenPlugin::SetLooping(
    const LoopingMessage &msg) {
  auto iter = players_.find(msg.texture_id());
  if (iter == players_.end()) {
    return FlutterError("Invalid argument", "Player not found.");
  }
  try {
    iter->second->SetLooping(msg.is_looping());
  } catch (const VideoPlayerError &error) {
    return FlutterError(error.code(), error.message());
  }

  return std::nullopt;
}

std::optional<FlutterError> VideoPlayerTizenPlugin::SetVolume(
    const VolumeMessage &msg) {
  auto iter = players_.find(msg.texture_id());
  if (iter == players_.end()) {
    return FlutterError("Invalid argument", "Player not found.");
  }
  try {
    iter->second->SetVolume(msg.volume());
  } catch (const VideoPlayerError &error) {
    return FlutterError(error.code(), error.message());
  }

  return std::nullopt;
}

std::optional<FlutterError> VideoPlayerTizenPlugin::SetPlaybackSpeed(
    const PlaybackSpeedMessage &msg) {
  auto iter = players_.find(msg.texture_id());
  if (iter == players_.end()) {
    return FlutterError("Invalid argument", "Player not found.");
  }
  try {
    iter->second->SetPlaybackSpeed(msg.speed());
  } catch (const VideoPlayerError &error) {
    return FlutterError(error.code(), error.message());
  }

  return std::nullopt;
}

std::optional<FlutterError> VideoPlayerTizenPlugin::Play(
    const TextureMessage &msg) {
  auto iter = players_.find(msg.texture_id());
  if (iter == players_.end()) {
    return FlutterError("Invalid argument", "Player not found.");
  }
  try {
    iter->second->Play();
  } catch (const VideoPlayerError &error) {
    return FlutterError(error.code(), error.message());
  }

  return std::nullopt;
}

std::optional<FlutterError> VideoPlayerTizenPlugin::Pause(
    const TextureMessage &msg) {
  auto iter = players_.find(msg.texture_id());
  if (iter == players_.end()) {
    return FlutterError("Invalid argument", "Player not found.");
  }
  try {
    iter->second->Pause();
  } catch (const VideoPlayerError &error) {
    return FlutterError(error.code(), error.message());
  }

  return std::nullopt;
}

ErrorOr<PositionMessage> VideoPlayerTizenPlugin::Position(
    const TextureMessage &msg) {
  auto iter = players_.find(msg.texture_id());
  if (iter == players_.end()) {
    return FlutterError("Invalid argument", "Player not found.");
  }

  int32_t position = 0;
  try {
    position = iter->second->GetPosition();
  } catch (const VideoPlayerError &error) {
    return FlutterError(error.code(), error.message());
  }

  PositionMessage result(msg.texture_id(), position);
  return result;
}

void VideoPlayerTizenPlugin::SeekTo(
    const PositionMessage &msg,
    std::function<void(std::optional<FlutterError> reply)> result) {
  auto iter = players_.find(msg.texture_id());
  if (iter == players_.end()) {
    result(FlutterError("Invalid argument", "Player not found."));
    return;
  }
  try {
    iter->second->SeekTo(msg.position(),
                         [result]() -> void { result(std::nullopt); });
  } catch (const VideoPlayerError &error) {
    result(FlutterError(error.code(), error.message()));
  }
}

std::optional<FlutterError> VideoPlayerTizenPlugin::SetMixWithOthers(
    const MixWithOthersMessage &msg) {
  options_.SetMixWithOthers(msg.mix_with_others());
  return std::nullopt;
}

std::optional<FlutterError> VideoPlayerTizenPlugin::SetPreventsDisplaySleep(
    const PreventsDisplaySleepMessage &msg) {
  auto iter = players_.find(msg.texture_id());
  if (iter == players_.end()) {
    return FlutterError("Invalid argument", "Player not found.");
  }
  iter->second->SetPreventsDisplaySleep(msg.prevents_display_sleep());

  return std::nullopt;
}

ErrorOr<flutter::EncodableList> VideoPlayerTizenPlugin::GetAudioTracks(
    const TextureMessage &msg) {
  auto iter = players_.find(msg.texture_id());
  if (iter == players_.end()) {
    return FlutterError("Invalid argument", "Player not found.");
  }

  flutter::EncodableList result;
  try {
    for (const AudioTrack &track : iter->second->GetAudioTracks()) {
      AudioTrackMessage message(track.index, track.is_selected);
      if (track.language) {
        message.set_language(*track.language);
      }
      result.push_back(flutter::CustomEncodableValue(message));
    }
  } catch (const VideoPlayerError &error) {
    return FlutterError(error.code(), error.message());
  }
  return result;
}

std::optional<FlutterError> VideoPlayerTizenPlugin::SelectAudioTrack(
    const SelectAudioTrackMessage &msg) {
  auto iter = players_.find(msg.texture_id());
  if (iter == players_.end()) {
    return FlutterError("Invalid argument", "Player not found.");
  }
  try {
    iter->second->SelectAudioTrack(msg.track_index());
  } catch (const VideoPlayerError &error) {
    return FlutterError(error.code(), error.message());
  }

  return std::nullopt;
}

}  // namespace video_player_tizen

void VideoPlayerTizenPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  video_player_tizen::VideoPlayer::SetRegistrarRef(registrar);
  video_player_tizen::VideoPlayerTizenPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrar>(registrar));
}

// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:flutter/foundation.dart' show debugPrint, kReleaseMode;
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:video_player_platform_interface/video_player_platform_interface.dart'
    as platform_interface;

import 'src/messages.g.dart';
import 'src/text_tracks.dart';

export 'src/text_tracks.dart' show TizenSubtitle, TizenTextTrack;
import 'src/video_hole.dart';

// TODO(JSUYA): Remove the ignore and rename parameters when adding support for platform views.
// ignore_for_file: avoid_renaming_method_parameters

/// A Tizen implementation of [VideoPlayerPlatform] that uses the
/// Pigeon-generated [TizenVideoPlayerApi].
class VideoPlayerTizen extends platform_interface.VideoPlayerPlatform {
  // ziptv2: players asked for the video hole, keyed by texture id; the value
  // turns false when the native side fell back to the texture.
  final Map<int, ValueNotifier<bool>> _videoHoles = <int, ValueNotifier<bool>>{};

  // ziptv2: embedded text subtitles, keyed by texture id.
  static const MethodChannel _tracksChannel = MethodChannel('ziptv2/video_player_tizen/tracks');
  final Map<int, StreamController<TizenSubtitle>> _subtitles =
      <int, StreamController<TizenSubtitle>>{};

  /// Embedded text subtitle tracks of the player [textureId].
  Future<List<TizenTextTrack>> getTextTracks(int textureId) async {
    final List<Object?>? raw = await _tracksChannel.invokeMethod<List<Object?>>(
      'getTextTracks',
      <String, Object>{'textureId': textureId},
    );
    return <TizenTextTrack>[
      for (final Object? item in raw ?? const <Object?>[])
        if (item is Map)
          TizenTextTrack(
            index: item['index'] as int,
            isSelected: item['isSelected'] == true,
            language: item['language'] as String?,
          ),
    ];
  }

  /// Selects the text track [index] of the player [textureId]; `null` turns
  /// subtitles off.
  Future<void> selectTextTrack(int textureId, int? index) {
    return _tracksChannel.invokeMethod<void>('selectTextTrack', <String, Object>{
      'textureId': textureId,
      'index': index ?? -1,
    });
  }

  /// Subtitle texts of the player [textureId] while a text track is selected.
  Stream<TizenSubtitle> subtitlesFor(int textureId) => _subtitlesController(textureId).stream;

  StreamController<TizenSubtitle> _subtitlesController(int textureId) =>
      _subtitles.putIfAbsent(textureId, StreamController<TizenSubtitle>.broadcast);

  final TizenVideoPlayerApi _api = TizenVideoPlayerApi();

  /// Registers this class as the default platform instance.
  static void register() {
    platform_interface.VideoPlayerPlatform.instance = VideoPlayerTizen();
  }

  @override
  Future<void> init() {
    return _api.initialize();
  }

  @override
  Future<void> dispose(int textureId) {
    _videoHoles.remove(textureId)?.dispose();
    _subtitles.remove(textureId)?.close();
    return _api.dispose(TextureMessage(textureId: textureId));
  }

  @override
  Future<int?> create(platform_interface.DataSource dataSource) async {
    String? asset;
    String? packageName;
    String? uri;
    String? formatHint;
    var httpHeaders = <String, String>{};
    switch (dataSource.sourceType) {
      case platform_interface.DataSourceType.asset:
        asset = dataSource.asset;
        packageName = dataSource.package;
      case platform_interface.DataSourceType.network:
        uri = dataSource.uri;
        formatHint = _videoFormatStringMap[dataSource.formatHint];
        httpHeaders = dataSource.httpHeaders;
      case platform_interface.DataSourceType.file:
        uri = dataSource.uri;
      case platform_interface.DataSourceType.contentUri:
        uri = dataSource.uri;
    }
    final message = CreateMessage(
      asset: asset,
      packageName: packageName,
      uri: uri,
      httpHeaders: httpHeaders,
      formatHint: formatHint,
    );

    final TextureMessage response = await _api.create(message);
    // ziptv2: video hole requested; the initialized event confirms it.
    if (httpHeaders.containsKey(videoHoleHeader)) {
      _videoHoles[response.textureId] = ValueNotifier<bool>(true);
    }
    return response.textureId;
  }

  @override
  Future<void> setLooping(int textureId, bool looping) {
    return _api.setLooping(LoopingMessage(textureId: textureId, isLooping: looping));
  }

  @override
  Future<void> play(int textureId) {
    return _api.play(TextureMessage(textureId: textureId));
  }

  @override
  Future<void> pause(int textureId) {
    return _api.pause(TextureMessage(textureId: textureId));
  }

  @override
  Future<void> setVolume(int textureId, double volume) {
    return _api.setVolume(VolumeMessage(textureId: textureId, volume: volume));
  }

  @override
  Future<void> setPlaybackSpeed(int textureId, double speed) {
    assert(speed > 0);

    return _api.setPlaybackSpeed(PlaybackSpeedMessage(textureId: textureId, speed: speed));
  }

  @override
  Future<void> seekTo(int textureId, Duration position) {
    return _api.seekTo(PositionMessage(textureId: textureId, position: position.inMilliseconds));
  }

  @override
  Future<Duration> getPosition(int textureId) async {
    final PositionMessage response = await _api.position(TextureMessage(textureId: textureId));
    return Duration(milliseconds: response.position);
  }

  @override
  Stream<platform_interface.VideoEvent> videoEventsFor(int textureId) {
    return _eventChannelFor(textureId).receiveBroadcastStream().map((dynamic event) {
      final map = event as Map<dynamic, dynamic>;
      switch (map['event']) {
        case 'initialized':
          // ziptv2: back to the texture if the native player could not set
          // up the video hole.
          _videoHoles[textureId]?.value = map['videoHole'] == true;
          // ziptv2: streaming buffering time read back from the native player
          // (-1 = not requested or not readable), logged outside release.
          if (!kReleaseMode && map['bufferingPrebufferMs'] != null) {
            debugPrint(
              '[video_player_tizen] buffering '
              '${map['bufferingPrebufferMs']}/${map['bufferingRebufferMs']} ms',
            );
          }
          return platform_interface.VideoEvent(
            eventType: platform_interface.VideoEventType.initialized,
            duration: Duration(milliseconds: map['duration'] as int),
            size: Size(
              (map['width'] as num?)?.toDouble() ?? 0.0,
              (map['height'] as num?)?.toDouble() ?? 0.0,
            ),
            rotationCorrection: map['rotationCorrection'] as int? ?? 0,
          );
        case 'completed':
          return platform_interface.VideoEvent(
            eventType: platform_interface.VideoEventType.completed,
          );
        case 'bufferingUpdate':
          final values = map['values'] as List<dynamic>;

          return platform_interface.VideoEvent(
            buffered: values.map<platform_interface.DurationRange>(_toDurationRange).toList(),
            eventType: platform_interface.VideoEventType.bufferingUpdate,
          );
        case 'bufferingStart':
          return platform_interface.VideoEvent(
            eventType: platform_interface.VideoEventType.bufferingStart,
          );
        case 'bufferingEnd':
          return platform_interface.VideoEvent(
            eventType: platform_interface.VideoEventType.bufferingEnd,
          );
        case 'isPlayingStateUpdate':
          return platform_interface.VideoEvent(
            eventType: platform_interface.VideoEventType.isPlayingStateUpdate,
            isPlaying: map['isPlaying'] as bool,
          );
        case 'subtitle':
          // ziptv2: not a video_player event; forwarded to subtitlesFor.
          _subtitlesController(textureId).add(
            TizenSubtitle(
              text: map['text'] as String? ?? '',
              duration: Duration(milliseconds: map['duration'] as int? ?? 0),
            ),
          );
          return platform_interface.VideoEvent(
            eventType: platform_interface.VideoEventType.unknown,
          );
        default:
          return platform_interface.VideoEvent(
            eventType: platform_interface.VideoEventType.unknown,
          );
      }
    });
  }

  @override
  Widget buildView(int textureId) {
    final ValueNotifier<bool>? hole = _videoHoles[textureId];
    if (hole == null) {
      return Texture(textureId: textureId);
    }
    return ValueListenableBuilder<bool>(
      valueListenable: hole,
      builder: (BuildContext context, bool isHole, Widget? _) => isHole
          ? VideoHoleView(textureId: textureId)
          : Texture(textureId: textureId),
    );
  }

  @override
  Future<void> setMixWithOthers(bool mixWithOthers) {
    return _api.setMixWithOthers(MixWithOthersMessage(mixWithOthers: mixWithOthers));
  }

  @override
  Future<void> setPreventsDisplaySleepDuringVideoPlayback(
    int textureId,
    bool preventsDisplaySleepDuringVideoPlayback,
  ) {
    return _api.setPreventsDisplaySleep(
      PreventsDisplaySleepMessage(
        textureId: textureId,
        preventsDisplaySleep: preventsDisplaySleepDuringVideoPlayback,
      ),
    );
  }

  @override
  Future<List<platform_interface.VideoAudioTrack>> getAudioTracks(int textureId) async {
    final List<AudioTrackMessage> tracks = await _api.getAudioTracks(
      TextureMessage(textureId: textureId),
    );
    return tracks
        .map(
          (AudioTrackMessage track) => platform_interface.VideoAudioTrack(
            id: track.trackIndex.toString(),
            label: null,
            language: track.language,
            isSelected: track.isSelected,
          ),
        )
        .toList();
  }

  @override
  Future<void> selectAudioTrack(int textureId, String trackId) {
    return _api.selectAudioTrack(
      SelectAudioTrackMessage(textureId: textureId, trackIndex: int.parse(trackId)),
    );
  }

  @override
  bool isAudioTrackSupportAvailable() {
    return true;
  }

  EventChannel _eventChannelFor(int textureId) {
    return EventChannel('flutter.io/videoPlayer/videoEvents$textureId');
  }

  static const Map<platform_interface.VideoFormat, String> _videoFormatStringMap =
      <platform_interface.VideoFormat, String>{
        platform_interface.VideoFormat.ss: 'ss',
        platform_interface.VideoFormat.hls: 'hls',
        platform_interface.VideoFormat.dash: 'dash',
        platform_interface.VideoFormat.other: 'other',
      };

  platform_interface.DurationRange _toDurationRange(dynamic value) {
    final pair = value as List<dynamic>;
    return platform_interface.DurationRange(
      Duration(milliseconds: pair[0] as int),
      Duration(milliseconds: pair[1] as int),
    );
  }
}

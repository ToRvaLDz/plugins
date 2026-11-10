// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ziptv2: embedded text subtitles, outside the video_player platform
// interface (it has no subtitle API).

import 'package:flutter/foundation.dart';

/// An embedded text subtitle track of a player.
@immutable
class TizenTextTrack {
  /// Creates a [TizenTextTrack].
  const TizenTextTrack({required this.index, required this.isSelected, this.language});

  /// Index of the track, as passed to `selectTextTrack`.
  final int index;

  /// Whether the track is selected and subtitles are on.
  final bool isSelected;

  /// Language code reported by the native player, if any.
  final String? language;
}

/// A subtitle text to show for [duration] from now.
@immutable
class TizenSubtitle {
  /// Creates a [TizenSubtitle].
  const TizenSubtitle({required this.text, required this.duration});

  /// The text, possibly with line breaks.
  final String text;

  /// How long the text stays on screen.
  final Duration duration;
}

// Copyright 2023 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ziptv2: video hole, adapted from video_player_videohole. The video renders
// on the hardware video plane below the Flutter surface; this widget punches a
// transparent hole where the video goes and reports that area, in physical
// pixels, to the native player at every frame where it changed.

import 'dart:ui' as ui;

import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

/// Reserved header asking the native player for the video hole, with the
/// window geometry in pixels: `<x>,<y>,<width>,<height>`.
const String videoHoleHeader = 'X-Tizen-Video-Hole';

const MethodChannel _channel = MethodChannel('ziptv2/video_player_tizen/video_hole');

/// Shows the video of [textureId] through a hole in the Flutter UI.
class VideoHoleView extends StatefulWidget {
  /// Creates a [VideoHoleView].
  const VideoHoleView({super.key, required this.textureId});

  /// The player whose display area follows this widget.
  final int textureId;

  @override
  State<VideoHoleView> createState() => _VideoHoleViewState();
}

class _VideoHoleViewState extends State<VideoHoleView> {
  final GlobalKey _boxKey = GlobalKey();
  Rect _sentRect = Rect.zero;
  bool _active = true;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback(_afterFrame);
  }

  @override
  void didUpdateWidget(VideoHoleView oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.textureId != widget.textureId) {
      _sentRect = Rect.zero;
    }
  }

  @override
  void dispose() {
    _active = false;
    // The video plane stays on screen otherwise: shrink it out of sight.
    _send(widget.textureId, const Rect.fromLTWH(0, 0, 1, 1));
    super.dispose();
  }

  void _afterFrame(Duration _) {
    if (!_active || !mounted) {
      return;
    }
    final Rect rect = _currentRect();
    if (rect != _sentRect && rect.width >= 1 && rect.height >= 1) {
      _sentRect = rect;
      _send(widget.textureId, rect);
    }
    WidgetsBinding.instance.addPostFrameCallback(_afterFrame);
    // Keeps frames coming so that a move without rebuild is still followed.
    WidgetsBinding.instance.scheduleFrame();
  }

  Rect _currentRect() {
    final RenderObject? object = _boxKey.currentContext?.findRenderObject();
    if (object is! RenderBox || !object.hasSize || !object.attached) {
      return Rect.zero;
    }
    final double ratio = View.of(context).devicePixelRatio;
    final Offset offset = object.localToGlobal(Offset.zero) * ratio;
    final Size size = object.size * ratio;
    final Rect rect = offset & size;
    if (!rect.isFinite) {
      return Rect.zero;
    }
    return Rect.fromLTRB(
      rect.left.roundToDouble(),
      rect.top.roundToDouble(),
      rect.right.roundToDouble(),
      rect.bottom.roundToDouble(),
    );
  }

  static void _send(int textureId, Rect rect) {
    _channel
        .invokeMethod<void>('setDisplayRoi', <String, int>{
          'textureId': textureId,
          'x': rect.left.toInt(),
          'y': rect.top.toInt(),
          'width': rect.width.toInt(),
          'height': rect.height.toInt(),
        })
        .catchError((Object _) {});
  }

  @override
  Widget build(BuildContext context) {
    return SizedBox.expand(key: _boxKey, child: const _Hole());
  }
}

/// A widget that creates a transparent hole in the Flutter UI.
class _Hole extends LeafRenderObjectWidget {
  const _Hole();

  @override
  RenderBox createRenderObject(BuildContext context) => _HoleBox();
}

class _HoleBox extends RenderBox {
  @override
  bool get sizedByParent => true;

  @override
  bool get alwaysNeedsCompositing => true;

  @override
  bool get isRepaintBoundary => true;

  @override
  void performResize() {
    size = constraints.biggest;
  }

  @override
  bool hitTestSelf(Offset position) => true;

  @override
  void paint(PaintingContext context, Offset offset) {
    context.addLayer(_HoleLayer(rect: offset & size));
  }
}

/// A composite layer that clears its rect to transparent.
class _HoleLayer extends Layer {
  _HoleLayer({required this.rect});

  final Rect rect;

  @override
  void addToScene(ui.SceneBuilder builder) {
    final recorder = ui.PictureRecorder();
    final canvas = Canvas(recorder);
    canvas.drawRect(
      rect,
      Paint()
        ..color = const Color(0x00000000)
        ..blendMode = BlendMode.src,
    );
    builder.addPicture(Offset.zero, recorder.endRecording());
  }
}

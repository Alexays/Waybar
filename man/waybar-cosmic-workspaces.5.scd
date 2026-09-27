waybar-cosmic-workspaces(5)

# NAME

waybar - COSMIC workspace thumbnails module

# DESCRIPTION

The *workspaces* module displays a preview for each workspace, letting you
see what's happening on a workspace before switching to it, either as a live,
continuously-updating screencopy thumbnail or a plain themable rectangle.

Live previews only work under the COSMIC desktop's compositor (cosmic-comp).
They require the compositor to implement the *ext-workspace-v1*,
*ext-image-copy-capture-v1* and COSMIC's workspace image-capture-source
Wayland protocols. Standard (non-live) mode only requires *ext-workspace-v1*
and works on any compositor that implements it.

# CONFIGURATION

Addressed by *cosmic/workspaces*

*display*: ++
	typeof: string ++
	default: standard ++
	Either *live* (real-time screencopy previews) or *standard* (a plain,
	CSS-styled rectangle per workspace).

*thumbnail-width*: ++
	typeof: int ++
	default: 160 ++
	Width in pixels of each workspace thumbnail.

*thumbnail-height*: ++
	typeof: int ++
	default: 90 ++
	Height in pixels of each workspace thumbnail.

*workspace-wrap-size*: ++
	typeof: int ++
	default: 0 ++
	Maximum amount of the bar's main axis, in pixels, that a single row
	(horizontal bar) or column (vertical bar) of thumbnails may consume,
	measured at the configured *thumbnail-width*/*thumbnail-height* including
	*spacing* between items. When the visible workspaces would need more than
	this, enough rows/columns are added to fit them, and *thumbnail-width* and
	*thumbnail-height* are both divided by the number of rows/columns (subject
	to *thumbnail-min-width*/*thumbnail-min-height*). A value of 0 disables
	wrapping.

*thumbnail-min-width*: ++
	typeof: int ++
	default: 0 ++
	Floor, in pixels, below which *workspace-wrap-size* will not shrink
	*thumbnail-width*. Only relevant when *workspace-wrap-size* is set.

*thumbnail-min-height*: ++
	typeof: int ++
	default: 0 ++
	Floor, in pixels, below which *workspace-wrap-size* will not shrink
	*thumbnail-height*. Only relevant when *workspace-wrap-size* is set.

*interval*: ++
	typeof: int ++
	default: 1000 ++
	Minimum delay, in milliseconds, between two captures of the same workspace.
	Only relevant when *display* is *live*.

*all-outputs*: ++
	typeof: bool ++
	default: false ++
	If set to false, only workspaces assigned to this bar's output are shown.
	Otherwise, workspaces from every output are shown.

*crop-dead-zones*: ++
	typeof: bool|string ++
	default: own-bar ++
	When *display* is *live*, crops out the portion of the preview that is
	obscured by exclusive-zone bars/panels/docks on the workspace's output.
	One of *own-bar* (crop only this bar's own reserved strip), *all-bars*
	(crop every exclusive-zone layer-surface on the output), or *none*
	(don't crop). For backwards compatibility, *true* is equivalent to
	*own-bar* and *false* to *none*.

	Requires the compositor to support *wlr-layer-shell-unstable-v1* and
	COSMIC's *cosmic-overlap-notify-unstable-v1* protocol; otherwise this
	option has no effect.

*highlight-show*: ++
	typeof: bool ++
	default: true ++
	Draws a border around the currently active workspace's thumbnail. The
	border's colour is themed via CSS (see *STYLE*); its thickness is set with
	*highlight-line-size*.

*highlight-line-size*: ++
	typeof: int ++
	default: 4 ++
	Thickness, in pixels, of the *highlight-show* border. Set to 0 to disable
	the border without touching *highlight-show*.

*mouse-scroll-enabled*: ++
	typeof: bool ++
	default: true ++
	Scrolling over the module steps through the visible workspaces.

*thumbnail-label-show*: ++
	typeof: bool ++
	default: true ++
	Overlays a text label on top of each thumbnail/rectangle.

*thumbnail-label*: ++
	typeof: string ++
	default: "{number}" ++
	Format string used both for the overlaid thumbnail label and the adjacent
	label (see *label-show*). Supports *{name}* (the workspace's name) and
	*{number}* (its 1-based position among the workspaces shown on this bar).

*thumbnail-label-horizontal-align*, *thumbnail-label-vertical-align*: ++
	typeof: string ++
	default: center ++
	Alignment of the overlaid thumbnail label. One of *start*, *center*, *end*.

*label-show*: ++
	typeof: bool ++
	default: true ++
	Shows a separate label alongside (not overlaid on) each thumbnail.

*label-position*: ++
	typeof: string ++
	default: before ++
	Either *before* or *after* the thumbnail. "Before" is above the thumbnail
	for a vertical (left/right) bar, or to its left for a horizontal
	(top/bottom) bar; "after" is the opposite.

*spacing*: ++
	typeof: int ++
	default: 4 ++
	Gap, in pixels, between thumbnail items.

# STYLE

- *#cosmic-workspaces*
- *#cosmic-workspaces .workspace* (the box wrapping one workspace's thumbnail and adjacent label)
- *#cosmic-workspaces .thumbnail* (the thumbnail/rectangle itself; background and border are
  CSS-driven, so this is how *display: standard*'s rectangle colour is set)
- *#cosmic-workspaces .thumbnail.active* (the active workspace's thumbnail, when *highlight-show*
  is enabled; set *border-color* here to theme the highlight drawn by *highlight-line-size*)
- *#cosmic-workspaces .thumbnail-label* (the label overlaid on the thumbnail)
- *#cosmic-workspaces .label* (the adjacent label placed before/after the thumbnail)

# SEE ALSO

waybar(5)

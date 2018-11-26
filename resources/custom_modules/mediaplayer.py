#!/usr/bin/env python3
import sys
import signal
import gi
gi.require_version('Playerctl', '2.0')
from gi.repository import Playerctl, GLib

manager = Playerctl.PlayerManager()
loop = GLib.MainLoop()


def on_play(player, status, manager):
    on_metadata(player, player.props.metadata, manager)


def on_metadata(player, metadata, manager):
    track_info = ''

    if player.props.player_name == 'spotify' and \
            'mpris:trackid' in metadata.keys() and \
            ':ad:' in player.props.metadata['mpris:trackid']:
        track_info = 'AD PLAYING'
    elif player.get_artist() != '' and player.get_title() != '':
        track_info = '{artist} - {title}'.format(artist=player.get_artist(),
                                                 title=player.get_title())
    else:
        sys.stdout.write('\n')
        sys.stdout.flush()
        return

    if player.props.status == 'Playing':
        sys.stdout.write(track_info + '\n')
    else:
        sys.stdout.write(' ' + track_info + '\n')
    sys.stdout.flush()


def on_name_appeared(manager, name):
    init_player(name)


def on_player_vanished(manager, player):
    sys.stdout.write("\n")
    sys.stdout.flush()


def init_player(name):
    player = Playerctl.Player.new_from_name(name)
    player.connect('playback-status', on_play, manager)
    player.connect('metadata', on_metadata, manager)
    manager.manage_player(player)
    on_metadata(player, player.props.metadata, manager)


def signal_handler(sig, frame):
    sys.stdout.write("\n")
    sys.stdout.flush()
    loop.quit()
    sys.exit(0)


manager.connect('name-appeared', on_name_appeared)
manager.connect('player-vanished', on_player_vanished)

signal.signal(signal.SIGINT, signal_handler)
signal.signal(signal.SIGTERM, signal_handler)

for player in manager.props.player_names:
    init_player(player)

loop.run()

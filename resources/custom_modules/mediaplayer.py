#!/usr/bin/env python3
import argparse
import logging
import sys
import signal
import gi
import json
import dbus
gi.require_version('Playerctl', '2.0')
from gi.repository import Playerctl, GLib
from dbus.mainloop.glib import DBusGMainLoop

logger = logging.getLogger(__name__)


def write_output(text, player):
    logger.info('Writing output')

    output = {'text': text,
              'class': 'custom-' + player.props.player_name,
              'alt': player.props.player_name}

    sys.stdout.write(json.dumps(output) + '\n')
    sys.stdout.flush()


def on_play(player, status, manager):
    logger.info('Received new playback status')
    on_metadata(player, player.props.metadata, manager)


def on_metadata(player, metadata, manager):
    logger.info('Received new metadata')
    track_info = ''

    if player.props.player_name == 'spotify' and \
            'mpris:trackid' in metadata.keys() and \
            ':ad:' in player.props.metadata['mpris:trackid']:
        track_info = 'AD PLAYING'
    elif player.get_artist() != '' and player.get_title() != '':
        track_info = '{artist} - {title}'.format(artist=player.get_artist(),
                                                 title=player.get_title())
    else:
        track_info = player.get_title()

    if player.props.status != 'Playing' and track_info:
        track_info = ' ' + track_info
    write_output(track_info, player)


def on_player_appeared(manager, player, selected_player=None):
    if player is not None and (selected_player is None or player.name == selected_player):
        init_player(manager, player)
    else:
        logger.debug("New player appeared, but it's not the selected player, skipping")


def on_player_vanished(manager, player):
    logger.info('Player has vanished')
    if len(manager.props.players) > 0:
        player = manager.props.players[0]
        on_metadata(player, player.props.metadata, manager)
        return
    sys.stdout.write('\n')
    sys.stdout.flush()


def init_player(manager, name):
    logger.debug('Initialize player: {player}'.format(player=name.name))
    player = Playerctl.Player.new_from_name(name)
    player.connect('playback-status', on_play, manager)
    player.connect('metadata', on_metadata, manager)
    manager.manage_player(player)
    on_metadata(player, player.props.metadata, manager)


def signal_handler(sig, frame):
    logger.debug('Received signal to stop, exiting')
    sys.stdout.write('\n')
    sys.stdout.flush()
    # loop.quit()
    sys.exit(0)


def dbus_signal_handler(*args, manager, **kwargs):
    # there is no implicit way to get current player,
    # so we need to create a new manager to get the right order of players
    new_manager = Playerctl.PlayerManager()
    if len(new_manager.props.player_names) > 1:
        current_player_name = new_manager.props.player_names[0].name
        for player in manager.props.players:
            if player.props.player_name == current_player_name:
                on_metadata(player, player.props.metadata, manager)


def parse_arguments():
    parser = argparse.ArgumentParser()

    # Increase verbosity with every occurrence of -v
    parser.add_argument('-v', '--verbose', action='count', default=0)

    # Define for which player we're listening
    parser.add_argument('--player')

    return parser.parse_args()


def main():
    arguments = parse_arguments()

    # Initialize logging
    logging.basicConfig(stream=sys.stderr, level=logging.DEBUG,
                        format='%(name)s %(levelname)s %(message)s')

    # Logging is set by default to WARN and higher.
    # With every occurrence of -v it's lowered by one
    logger.setLevel(max((3 - arguments.verbose) * 10, 0))

    # Log the sent command line arguments
    logger.debug('Arguments received {}'.format(vars(arguments)))

    manager = Playerctl.PlayerManager()
    loop = GLib.MainLoop()

    manager.connect('name-appeared', lambda *args: on_player_appeared(*args, arguments.player))
    manager.connect('player-vanished', on_player_vanished)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    signal.signal(signal.SIGUSR1, lambda *args: shift_signal_handler(*args, manager))

    # use dbus to shift player (e.g. after playerctld shift)
    DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()
    bus.add_signal_receiver(lambda *args, **kwargs: dbus_signal_handler(*args, **kwargs, manager=manager),
                            signal_name='Shift',
                            dbus_interface='org.waybar.Media')

    for player in manager.props.player_names:
        if arguments.player is not None and arguments.player != player.name:
            logger.debug('{player} is not the filtered player, skipping it'
                         .format(player=player.name)
                         )
            continue

        init_player(manager, player)

    loop.run()


if __name__ == '__main__':
    main()

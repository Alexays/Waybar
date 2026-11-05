#!/usr/bin/env python3
"""Fake compositor IPC servers for ipc.sh.

Usage: fake_ipc.py hyprland|niri|mango|umbriel|wayfire SOCKET...

Serves one compositor's IPC on SOCKET (Hyprland: request socket, then event
socket) from a small session: one output, a few workspaces, their windows and
a keyboard layout. Each stdin line is a step (an op_* method with shell-style
arguments); it changes the session and sends every subscribed client the
events the real compositor sends for it, then prints "ok". The step "exit"
closes every connection and removes the sockets, like a compositor going away.
"""
import json
import os
import shlex
import socket
import struct
import sys
import threading

OUTPUT = "HEADLESS-1"
WIDTH, HEIGHT = 1920, 1080
KEYBOARD = "at-translated-set-2-keyboard"
LAYOUTS = ["English (US)", "German"]


class Fake:
    last_id = 0
    layout = 0

    def __init__(self, paths):
        """Starts serving: subclasses build their session before calling it."""
        self.lock = threading.Lock()
        self.subscribers = []
        self.paths = paths
        for path, serve in zip(paths, self.servers()):
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.bind(path)
            sock.listen()
            threading.Thread(target=self.accept, args=(sock, serve), daemon=True).start()

    def accept(self, sock, serve):
        while True:
            conn, _ = sock.accept()
            threading.Thread(target=self.serve, args=(conn, serve), daemon=True).start()

    @staticmethod
    def serve(conn, handler):
        try:
            handler(conn)
        except OSError:
            conn.close()

    def new_id(self):
        self.last_id += 1
        return self.last_id

    def frame(self, message):
        return (json.dumps(message, separators=(",", ":")) + "\n").encode()

    def subscribe(self, conn, *messages):
        with self.lock:
            conn.sendall(b"".join(self.frame(m) for m in messages))
            self.subscribers.append(conn)

    def emit(self, *messages):
        data = b"".join(self.frame(m) for m in messages)
        for conn in list(self.subscribers):
            try:
                conn.sendall(data)
            except OSError:
                self.subscribers.remove(conn)

    def op_layout(self):
        self.layout = (self.layout + 1) % len(LAYOUTS)

    def run(self):
        print("ready", flush=True)
        for line in sys.stdin:
            op, *args = shlex.split(line)
            if op == "exit":
                break
            with self.lock:
                getattr(self, "op_" + op)(*args)
            print("ok", flush=True)
        for path in self.paths:
            os.unlink(path)
        # Exiting closes every connection at once, as a dying compositor does.
        os._exit(0)


def read_line(conn):
    data = b""
    while not data.endswith(b"\n"):
        chunk = conn.recv(4096)
        if not chunk:
            raise OSError("client closed")
        data += chunk
    return data.decode().strip()


class Hyprland(Fake):
    """Hyprland: hyprctl requests on .socket.sock, "EVENT>>DATA" lines on .socket2.sock."""

    def __init__(self, paths):
        self.workspaces = {1: [self.window("foot", "foot ~")],
                           2: [self.window("firefox", "Waybar wiki")]}
        self.active = 1
        super().__init__(paths)

    def servers(self):
        return [self.reply, self.subscribe]

    def frame(self, message):
        return (message + "\n").encode()

    def window(self, app, title):
        return {"address": f"0x55d1a0{self.new_id():04x}", "class": app, "title": title}

    def focused(self):
        windows = self.workspaces[self.active]
        return windows[-1] if windows else None

    def workspace_json(self, ws):
        windows = self.workspaces[ws]
        last = windows[-1] if windows else {"address": "0x0", "title": ""}
        return {"id": ws, "name": str(ws), "monitor": OUTPUT, "monitorID": 0,
                "windows": len(windows), "hasfullscreen": False,
                "lastwindow": last["address"], "lastwindowtitle": last["title"],
                "ispersistent": False}

    def reply(self, conn):
        request = conn.recv(4096).decode()
        with self.lock:
            if request == "j/workspaces":
                body = json.dumps([self.workspace_json(ws) for ws in self.workspaces])
            elif request == "j/activeworkspace":
                body = json.dumps(self.workspace_json(self.active))
            elif request == "j/monitors":
                body = json.dumps([{
                    "id": 0, "name": OUTPUT, "description": "Headless output", "make": "",
                    "model": "", "serial": "", "width": WIDTH, "height": HEIGHT,
                    "refreshRate": 60.0, "x": 0, "y": 0,
                    "activeWorkspace": {"id": self.active, "name": str(self.active)},
                    "specialWorkspace": {"id": 0, "name": ""}, "reserved": [0, 30, 0, 0],
                    "scale": 1.0, "transform": 0, "focused": True, "dpmsStatus": True,
                    "vrr": False, "disabled": False}])
            elif request == "j/clients":
                body = json.dumps([{
                    "address": w["address"], "mapped": True, "hidden": False, "at": [0, 30],
                    "size": [WIDTH, HEIGHT - 30], "workspace": {"id": ws, "name": str(ws)},
                    "floating": False, "pseudo": False, "monitor": 0, "class": w["class"],
                    "title": w["title"], "initialClass": w["class"], "initialTitle": w["title"],
                    "pid": 1000, "xwayland": False, "pinned": False, "fullscreen": 0,
                    "fullscreenClient": 0, "grouped": [], "tags": [], "swallowing": "0x0",
                    "focusHistoryID": 0}
                    for ws, windows in self.workspaces.items() for w in windows])
            elif request == "j/workspacerules":
                body = "[]"
            elif request == "devices":
                body = ("mice:\n\nKeyboards:\n"
                        "\tKeyboard at 55d1a0c0e000:\n"
                        f"\t\t{KEYBOARD}\n"
                        '\t\t\trules: r "", m "", l "us,de", v "", o ""\n'
                        f"\t\t\tactive keymap: {LAYOUTS[self.layout]}\n"
                        "\t\t\tcapsLock: no\n\t\t\tnumLock: no\n\t\t\tmain: yes\n")
            else:
                body = "unknown request"
        conn.sendall(body.encode())
        conn.close()

    def emit_focus(self):
        w = self.focused()
        if w:
            self.emit(f"activewindow>>{w['class']},{w['title']}",
                      f"activewindowv2>>{w['address'][2:]}")
        else:
            self.emit("activewindow>>,", "activewindowv2>>")

    def op_workspace(self, name):
        ws, previous = int(name), self.active
        if ws not in self.workspaces:
            self.workspaces[ws] = []
            self.emit(f"createworkspace>>{ws}", f"createworkspacev2>>{ws},{ws}")
        self.active = ws
        self.emit(f"workspace>>{ws}", f"workspacev2>>{ws},{ws}")
        self.emit_focus()
        if not self.workspaces[previous]:
            del self.workspaces[previous]
            self.emit(f"destroyworkspace>>{previous}",
                      f"destroyworkspacev2>>{previous},{previous}")

    def op_open(self, app, title):
        w = self.window(app, title)
        self.workspaces[self.active].append(w)
        self.emit(f"openwindow>>{w['address'][2:]},{self.active},{app},{title}")
        self.emit_focus()

    def op_title(self, title):
        w = self.focused()
        w["title"] = title
        self.emit(f"windowtitle>>{w['address'][2:]}",
                  f"windowtitlev2>>{w['address'][2:]},{title}")
        self.emit_focus()

    def op_close(self):
        w = self.workspaces[self.active].pop()
        self.emit(f"closewindow>>{w['address'][2:]}")
        self.emit_focus()

    def op_layout(self):
        super().op_layout()
        self.emit(f"activelayout>>{KEYBOARD},{LAYOUTS[self.layout]}")

    def op_submap(self, name=""):
        self.emit(f"submap>>{name}")


class Niri(Fake):
    """niri: JSON requests, one per connection; "EventStream" turns it into an event stream.

    Like niri, the output always ends with one empty workspace, and an empty
    workspace that is not the last one goes away once it loses focus.
    """

    def __init__(self, paths):
        self.workspaces = [self.workspace(), self.workspace(), self.workspace()]
        self.active = self.workspaces[0]
        self.windows = []
        self.add_window(self.workspaces[0], "foot", "foot ~")
        self.add_window(self.workspaces[1], "firefox", "Waybar wiki")
        super().__init__(paths)

    def servers(self):
        return [self.request]

    def workspace(self):
        return {"id": self.new_id(), "active_window_id": None}

    def add_window(self, ws, app, title):
        w = {"id": self.new_id(), "title": title, "app_id": app, "pid": 1000,
             "workspace_id": ws["id"], "is_floating": False, "is_urgent": False,
             "layout": {"pos_in_scrolling_layout": [1, 1], "tile_size": [WIDTH, HEIGHT - 30],
                        "window_size": [WIDTH, HEIGHT - 30], "tile_pos_in_workspace_view": None,
                        "window_offset_in_tile": [0, 0]}}
        self.windows.append(w)
        ws["active_window_id"] = w["id"]
        return w

    def window_json(self, w):
        return dict(w, is_focused=w["id"] == self.active["active_window_id"])

    def workspaces_changed(self):
        return {"WorkspacesChanged": {"workspaces": [
            {"id": ws["id"], "idx": i + 1, "name": None, "output": OUTPUT, "is_urgent": False,
             "is_active": ws is self.active, "is_focused": ws is self.active,
             "active_window_id": ws["active_window_id"]}
            for i, ws in enumerate(self.workspaces)]}}

    def request(self, conn):
        request = json.loads(read_line(conn))
        if request != "EventStream":
            conn.sendall(self.frame({"Err": "unsupported request"}))
            conn.close()
            return
        conn.sendall(self.frame({"Ok": "Handled"}))
        self.subscribe(
            conn, self.workspaces_changed(),
            {"WindowsChanged": {"windows": [self.window_json(w) for w in self.windows]}},
            {"KeyboardLayoutsChanged": {"keyboard_layouts": {"names": LAYOUTS,
                                                             "current_idx": self.layout}}},
            {"OverviewOpenedOrClosed": {"is_open": False}})

    def op_workspace(self, idx):
        previous, self.active = self.active, self.workspaces[int(idx) - 1]
        self.emit({"WorkspaceActivated": {"id": self.active["id"], "focused": True}},
                  {"WindowFocusChanged": {"id": self.active["active_window_id"]}})
        if previous["active_window_id"] is None and previous is not self.workspaces[-1]:
            self.workspaces.remove(previous)
            self.emit(self.workspaces_changed())

    def op_open(self, app, title):
        w = self.add_window(self.active, app, title)
        if self.active is self.workspaces[-1]:
            self.workspaces.append(self.workspace())
            self.emit(self.workspaces_changed())
        self.emit({"WindowOpenedOrChanged": {"window": self.window_json(w)}},
                  {"WorkspaceActiveWindowChanged": {"workspace_id": self.active["id"],
                                                    "active_window_id": w["id"]}},
                  {"WindowFocusChanged": {"id": w["id"]}})

    def op_title(self, title):
        w = next(w for w in self.windows if w["id"] == self.active["active_window_id"])
        w["title"] = title
        self.emit({"WindowOpenedOrChanged": {"window": self.window_json(w)}})

    def op_close(self):
        closed = self.active["active_window_id"]
        self.windows = [w for w in self.windows if w["id"] != closed]
        rest = [w["id"] for w in self.windows if w["workspace_id"] == self.active["id"]]
        self.active["active_window_id"] = rest[-1] if rest else None
        self.emit({"WindowClosed": {"id": closed}},
                  {"WorkspaceActiveWindowChanged": {
                      "workspace_id": self.active["id"],
                      "active_window_id": self.active["active_window_id"]}},
                  {"WindowFocusChanged": {"id": self.active["active_window_id"]}})

    def op_layout(self):
        super().op_layout()
        self.emit({"KeyboardLayoutSwitched": {"idx": self.layout}})


class Mango(Fake):
    """mango: "watch all-monitors" streams a full monitor snapshot on every change."""

    def __init__(self, paths):
        self.tags = {i: [] for i in range(1, 10)}
        self.tags[1].append(self.window("foot", "foot ~"))
        self.tags[2].append(self.window("firefox", "Waybar wiki"))
        self.active = 1
        self.keymode = "default"
        self.symbol = "T"
        super().__init__(paths)

    def servers(self):
        return [self.watch]

    def window(self, app, title):
        return {"id": self.new_id(), "title": title, "appid": app}

    def snapshot(self):
        windows = self.tags[self.active]
        client = None
        if windows:
            client = dict(windows[-1], tags=[self.active], monitor=OUTPUT)
        return {"monitors": [{
            "name": OUTPUT, "active": True, "active_tags": [self.active],
            "tags": [{"index": i, "is_active": i == self.active, "is_urgent": False,
                      "client_count": len(w)} for i, w in self.tags.items()],
            "active_client": client, "keyboardlayout": LAYOUTS[self.layout],
            "keymode": self.keymode, "layout_symbol": self.symbol}]}

    def watch(self, conn):
        if read_line(conn) != "watch all-monitors":
            conn.close()
            return
        self.subscribe(conn, self.snapshot())

    def op_workspace(self, tag):
        self.active = int(tag)
        self.emit(self.snapshot())

    def op_open(self, app, title):
        self.tags[self.active].append(self.window(app, title))
        self.emit(self.snapshot())

    def op_title(self, title):
        self.tags[self.active][-1]["title"] = title
        self.emit(self.snapshot())

    def op_close(self):
        self.tags[self.active].pop()
        self.emit(self.snapshot())

    def op_layout(self):
        super().op_layout()
        self.emit(self.snapshot())

    def op_keymode(self, mode):
        self.keymode = mode
        self.emit(self.snapshot())

    def op_arrange(self, symbol):
        self.symbol = symbol
        self.emit(self.snapshot())


class Umbriel(Fake):
    """Umbriel: a "subscribe" request streams a snapshot of each family on every change."""

    def __init__(self, paths):
        self.workspaces = [{"id": f"ws{i}", "windows": [], "layout": "scrolling"}
                           for i in range(1, 4)]
        self.workspaces[0]["windows"].append(self.window("foot", "foot ~"))
        self.workspaces[1]["windows"].append(self.window("firefox", "Waybar wiki"))
        self.active = self.workspaces[0]
        super().__init__(paths)

    def servers(self):
        return [self.request]

    def window(self, app, title):
        return {"id": str(self.new_id()), "title": title, "app_id": app}

    def workspaces_event(self):
        return {"event": "workspaces", "data": [
            {"id": ws["id"], "index": i + 1, "name": None, "named": False, "output": OUTPUT,
             "active": ws is self.active, "focused": ws is self.active, "urgent": False,
             "window_count": len(ws["windows"]), "layout": ws["layout"],
             "focused_window": ws["windows"][-1]["id"] if ws["windows"] else None}
            for i, ws in enumerate(self.workspaces)]}

    def windows_event(self):
        focused = self.active["windows"][-1] if self.active["windows"] else None
        return {"event": "windows", "data": [
            dict(w, workspace=ws["id"], active=w is focused, column=i)
            for ws in self.workspaces for i, w in enumerate(ws["windows"])]}

    def request(self, conn):
        request = json.loads(read_line(conn))
        if request.get("cmd") != "subscribe":
            conn.sendall(self.frame({"err": "unsupported request"}))
            conn.close()
            return
        self.subscribe(conn, self.workspaces_event(), self.windows_event())

    def op_workspace(self, idx):
        self.active = self.workspaces[int(idx) - 1]
        self.emit(self.workspaces_event(), self.windows_event())

    def op_open(self, app, title):
        self.active["windows"].append(self.window(app, title))
        self.emit(self.windows_event(), self.workspaces_event())

    def op_title(self, title):
        self.active["windows"][-1]["title"] = title
        self.emit(self.windows_event())

    def op_close(self):
        self.active["windows"].pop()
        self.emit(self.windows_event(), self.workspaces_event())

    def op_arrange(self, layout):
        self.active["layout"] = layout
        self.emit(self.workspaces_event())


class Wayfire(Fake):
    """Wayfire: length-prefixed JSON methods; "window-rules/events/watch" streams events.

    One output whose workspace set is a fixed 3x1 grid.
    """

    def __init__(self, paths):
        self.columns = [[self.view("foot", "foot ~", 0)],
                        [self.view("firefox", "Waybar wiki", 1)],
                        []]
        self.ws_x = 0
        super().__init__(paths)

    def servers(self):
        return [self.request]

    def frame(self, message):
        data = json.dumps(message, separators=(",", ":")).encode()
        return struct.pack("<I", len(data)) + data

    def view(self, app, title, column):
        return {"id": self.new_id(), "pid": 1000, "title": title, "app-id": app,
                "column": column, "mapped": True}

    def view_json(self, v):
        geometry = {"x": (v["column"] - self.ws_x) * WIDTH, "y": 30, "width": WIDTH,
                    "height": HEIGHT - 30}
        return {"id": v["id"], "pid": v["pid"], "title": v["title"], "app-id": v["app-id"],
                "base-geometry": geometry, "parent": -1, "geometry": geometry, "bbox": geometry,
                "output-id": 1, "output-name": OUTPUT, "last-focus-timestamp": 0,
                "role": "toplevel", "mapped": v["mapped"], "layer": "workspace",
                "tiled-edges": 15, "fullscreen": False, "minimized": False,
                "activated": v is self.focused(), "sticky": False, "wset-index": 1,
                "min-size": [0, 0], "max-size": [0, 0], "focusable": True, "type": "toplevel"}

    def focused(self):
        views = self.columns[self.ws_x]
        return views[-1] if views else None

    def grid(self):
        return {"x": self.ws_x, "y": 0, "grid_width": len(self.columns), "grid_height": 1}

    def output_json(self):
        return {"id": 1, "name": OUTPUT,
                "geometry": {"x": 0, "y": 0, "width": WIDTH, "height": HEIGHT},
                "workarea": {"x": 0, "y": 30, "width": WIDTH, "height": HEIGHT - 30},
                "wset-index": 1, "workspace": self.grid()}

    def wset_json(self):
        return {"index": 1, "name": "wset-1", "output-id": 1, "output-name": OUTPUT,
                "workspace": self.grid()}

    def focus_event(self):
        v = self.focused()
        return {"event": "view-focused", "view": self.view_json(v) if v else None}

    def request(self, conn):
        header = conn.recv(4, socket.MSG_WAITALL)
        if len(header) < 4:
            raise OSError("client closed")
        request = json.loads(conn.recv(struct.unpack("<I", header)[0], socket.MSG_WAITALL))
        method = request["method"]
        if method == "window-rules/events/watch":
            self.subscribe(conn, {"result": "ok"})
            return
        with self.lock:
            if method == "window-rules/list-outputs":
                reply = [self.output_json()]
            elif method == "window-rules/list-wsets":
                reply = [self.wset_json()]
            elif method == "window-rules/list-views":
                reply = [self.view_json(v) for column in self.columns for v in column]
            elif method == "window-rules/get-focused-view":
                v = self.focused()
                reply = {"result": "ok", "info": self.view_json(v) if v else None}
            elif method == "window-rules/get-focused-output":
                reply = {"result": "ok", "info": self.output_json()}
            else:
                reply = {"error": "No such method found!"}
        conn.sendall(self.frame(reply))
        conn.close()

    def op_workspace(self, idx):
        previous = {"x": self.ws_x, "y": 0}
        self.ws_x = int(idx) - 1
        switch = {"event": "plugin-activation-state-changed", "plugin": "vswitch",
                  "output": 1, "output-data": self.output_json()}
        self.emit(dict(switch, state=True),
                  {"event": "wset-workspace-changed", "previous-workspace": previous,
                   "new-workspace": {"x": self.ws_x, "y": 0}, "output": 1, "wset": "wset-1",
                   "output-data": self.output_json(), "wset-data": self.wset_json()},
                  self.focus_event(), dict(switch, state=False))

    def op_open(self, app, title):
        v = self.view(app, title, self.ws_x)
        self.columns[self.ws_x].append(v)
        self.emit({"event": "view-mapped", "view": self.view_json(v)}, self.focus_event())

    def op_title(self, title):
        v = self.focused()
        v["title"] = title
        self.emit({"event": "view-title-changed", "view": self.view_json(v)})

    def op_close(self):
        v = self.columns[self.ws_x].pop()
        v["mapped"] = False
        self.emit({"event": "view-unmapped", "view": self.view_json(v)}, self.focus_event())


if __name__ == "__main__":
    fakes = {"hyprland": Hyprland, "niri": Niri, "mango": Mango, "umbriel": Umbriel,
             "wayfire": Wayfire}
    fakes[sys.argv[1]](sys.argv[2:]).run()

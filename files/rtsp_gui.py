"""
RTSP Server Manager — Tkinter GUI
===================================
Affiche les flux RTSP multi-canaux, gère la configuration,
lance les streams dans VLC intégré ou externe.

Dépendances :
  pip install python-vlc watchdog

Utilisation :
  python rtsp_gui.py [chemin_config]
"""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext, colorchooser
import threading
import subprocess
import json
import os
import sys
import time
import socket
import datetime
import platform
from pathlib import Path
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

# ────────────────────────────────────────────────
# Chemin config par défaut
# ────────────────────────────────────────────────
DEFAULT_CONFIG = Path(__file__).parent / "config" / "rtsp_config.json"

# ────────────────────────────────────────────────
# Couleurs / Thème
# ────────────────────────────────────────────────
COLORS = {
    "bg":        "#0D1117",
    "sidebar":   "#161B22",
    "card":      "#21262D",
    "border":    "#30363D",
    "accent":    "#238636",
    "accent2":   "#1F6FEB",
    "danger":    "#DA3633",
    "warn":      "#D29922",
    "text":      "#E6EDF3",
    "text_dim":  "#8B949E",
    "green":     "#3FB950",
    "red":       "#F85149",
    "yellow":    "#E3B341",
    "blue":      "#79C0FF",
}

# ────────────────────────────────────────────────
# Config Manager
# ────────────────────────────────────────────────
class ConfigManager:
    def __init__(self, path: Path):
        self.path = path
        self.data = {}
        self.load()

    def load(self):
        try:
            with open(self.path, "r", encoding="utf-8") as f:
                self.data = json.load(f)
        except Exception as e:
            self.data = {
                "server": {"host": "127.0.0.1", "port": 8554},
                "channels": []
            }

    def save(self):
        with open(self.path, "w", encoding="utf-8") as f:
            json.dump(self.data, f, indent=2)

    @property
    def host(self):
        return self.data.get("server", {}).get("host", "127.0.0.1")

    @property
    def port(self):
        return self.data.get("server", {}).get("port", 8554)

    @property
    def channels(self):
        return self.data.get("channels", [])

    def add_channel(self, ch: dict):
        self.data.setdefault("channels", []).append(ch)
        self.save()

    def remove_channel(self, idx: int):
        self.data["channels"].pop(idx)
        self.save()

    def update_channel(self, idx: int, ch: dict):
        self.data["channels"][idx] = ch
        self.save()

    def rtsp_url(self, mount: str) -> str:
        return f"rtsp://{self.host}:{self.port}{mount}"


# ────────────────────────────────────────────────
# Config File Watcher (watchdog)
# ────────────────────────────────────────────────
class ConfigFileHandler(FileSystemEventHandler):
    def __init__(self, path: Path, callback):
        self.path = path.resolve()
        self.callback = callback

    def on_modified(self, event):
        if Path(event.src_path).resolve() == self.path:
            time.sleep(0.2)  # attendre fin d'écriture
            self.callback()


# ────────────────────────────────────────────────
# Log Widget
# ────────────────────────────────────────────────
class LogPanel(tk.Frame):
    def __init__(self, parent, **kw):
        super().__init__(parent, bg=COLORS["card"], **kw)
        header = tk.Label(self, text="📋  Journal système",
                          bg=COLORS["card"], fg=COLORS["blue"],
                          font=("Consolas", 11, "bold"), anchor="w")
        header.pack(fill="x", padx=10, pady=(8, 2))

        self.text = scrolledtext.ScrolledText(
            self, bg="#0A0D12", fg=COLORS["text"],
            font=("Consolas", 9), relief="flat",
            insertbackground=COLORS["text"],
            height=10
        )
        self.text.pack(fill="both", expand=True, padx=8, pady=4)
        self.text.tag_config("OK",   foreground=COLORS["green"])
        self.text.tag_config("ERR",  foreground=COLORS["red"])
        self.text.tag_config("WARN", foreground=COLORS["yellow"])
        self.text.tag_config("INFO", foreground=COLORS["blue"])

    def log(self, msg: str, level: str = "INFO"):
        ts  = datetime.datetime.now().strftime("%H:%M:%S")
        self.text.insert("end", f"[{ts}] [{level}] {msg}\n", level)
        self.text.see("end")

    def clear(self):
        self.text.delete("1.0", "end")


# ────────────────────────────────────────────────
# Channel Card Widget
# ────────────────────────────────────────────────
class ChannelCard(tk.Frame):
    def __init__(self, parent, channel: dict, idx: int,
                 on_edit, on_delete, on_vlc, on_copy, **kw):
        super().__init__(parent, bg=COLORS["card"],
                         highlightbackground=COLORS["border"],
                         highlightthickness=1, **kw)
        self.channel = channel
        self.idx     = idx

        enabled = channel.get("enabled", True)
        status_color = COLORS["green"] if enabled else COLORS["red"]
        status_sym   = "●" if enabled else "○"

        # ── Row 1: status + name + mount ──
        row1 = tk.Frame(self, bg=COLORS["card"])
        row1.pack(fill="x", padx=10, pady=(8, 2))

        tk.Label(row1, text=status_sym, fg=status_color,
                 bg=COLORS["card"], font=("Arial", 14)).pack(side="left")
        tk.Label(row1, text=f"  Ch {channel.get('id','?')}  ·  {channel.get('name','')}",
                 fg=COLORS["text"], bg=COLORS["card"],
                 font=("Consolas", 11, "bold")).pack(side="left")
        tk.Label(row1, text=channel.get("mount",""),
                 fg=COLORS["blue"], bg=COLORS["card"],
                 font=("Consolas", 10)).pack(side="left", padx=10)

        # ── Row 2: overlay text ──
        row2 = tk.Frame(self, bg=COLORS["card"])
        row2.pack(fill="x", padx=12, pady=2)
        tk.Label(row2, text="Overlay: ",
                 fg=COLORS["text_dim"], bg=COLORS["card"],
                 font=("Consolas", 9)).pack(side="left")
        tk.Label(row2, text=channel.get("text_overlay","—"),
                 fg=COLORS["yellow"], bg=COLORS["card"],
                 font=("Consolas", 9, "italic")).pack(side="left")

        # ── Row 3: buttons ──
        row3 = tk.Frame(self, bg=COLORS["card"])
        row3.pack(fill="x", padx=10, pady=(4, 8))

        self._btn(row3, "▶ VLC",    COLORS["accent"],  lambda: on_vlc(channel))
        self._btn(row3, "📋 Copier",COLORS["accent2"], lambda: on_copy(channel))
        self._btn(row3, "✏ Éditer", COLORS["warn"],    lambda: on_edit(idx))
        self._btn(row3, "✕ Suppr.", COLORS["danger"],  lambda: on_delete(idx))

    def _btn(self, parent, text, color, cmd):
        b = tk.Button(parent, text=text, bg=color, fg="white",
                      font=("Consolas", 9, "bold"), relief="flat",
                      padx=8, pady=3, cursor="hand2", command=cmd,
                      activebackground=color, activeforeground="white")
        b.pack(side="left", padx=(0, 6))


# ────────────────────────────────────────────────
# Channel Edit Dialog
# ────────────────────────────────────────────────
class ChannelDialog(tk.Toplevel):
    def __init__(self, parent, channel: dict = None, title="Nouveau canal"):
        super().__init__(parent)
        self.title(title)
        self.configure(bg=COLORS["bg"])
        self.resizable(False, False)
        self.result = None

        ch = channel or {
            "id": 1, "name": "Channel_1", "mount": "/stream1",
            "source": "videotestsrc", "pattern": 0,
            "text_overlay": "Live Stream", "enabled": True
        }

        fields = [
            ("ID",           "id",           str(ch.get("id", 1))),
            ("Nom",          "name",         ch.get("name", "")),
            ("Mount point",  "mount",        ch.get("mount", "")),
            ("Source",       "source",       ch.get("source", "videotestsrc")),
            ("Pattern (0-20)","pattern",     str(ch.get("pattern", 0))),
            ("Texte overlay","text_overlay", ch.get("text_overlay", "")),
        ]
        self.vars = {}
        for label, key, val in fields:
            row = tk.Frame(self, bg=COLORS["bg"])
            row.pack(fill="x", padx=20, pady=4)
            tk.Label(row, text=label, fg=COLORS["text_dim"],
                     bg=COLORS["bg"], width=16, anchor="w",
                     font=("Consolas", 9)).pack(side="left")
            v = tk.StringVar(value=val)
            self.vars[key] = v
            tk.Entry(row, textvariable=v, bg=COLORS["card"],
                     fg=COLORS["text"], insertbackground=COLORS["text"],
                     relief="flat", font=("Consolas", 10),
                     width=28).pack(side="left")

        # Enabled checkbox
        self.enabled_var = tk.BooleanVar(value=ch.get("enabled", True))
        ck = tk.Checkbutton(self, text="Activé", variable=self.enabled_var,
                            bg=COLORS["bg"], fg=COLORS["text"],
                            selectcolor=COLORS["card"],
                            font=("Consolas", 10), anchor="w")
        ck.pack(fill="x", padx=20, pady=4)

        # Buttons
        btn_frame = tk.Frame(self, bg=COLORS["bg"])
        btn_frame.pack(pady=12)
        tk.Button(btn_frame, text="✔ Valider", bg=COLORS["accent"],
                  fg="white", font=("Consolas", 10, "bold"), relief="flat",
                  padx=12, pady=4, cursor="hand2",
                  command=self._ok).pack(side="left", padx=6)
        tk.Button(btn_frame, text="✕ Annuler", bg=COLORS["danger"],
                  fg="white", font=("Consolas", 10, "bold"), relief="flat",
                  padx=12, pady=4, cursor="hand2",
                  command=self.destroy).pack(side="left", padx=6)
        self.grab_set()
        self.wait_window()

    def _ok(self):
        try:
            self.result = {
                "id":           int(self.vars["id"].get()),
                "name":         self.vars["name"].get(),
                "mount":        self.vars["mount"].get(),
                "source":       self.vars["source"].get(),
                "pattern":      int(self.vars["pattern"].get()),
                "text_overlay": self.vars["text_overlay"].get(),
                "enabled":      self.enabled_var.get(),
            }
        except ValueError as e:
            messagebox.showerror("Erreur", str(e), parent=self)
            return
        self.destroy()


# ────────────────────────────────────────────────
# Server Status Panel
# ────────────────────────────────────────────────
class StatusPanel(tk.Frame):
    def __init__(self, parent, cfg: ConfigManager, **kw):
        super().__init__(parent, bg=COLORS["sidebar"], **kw)
        self.cfg = cfg

        tk.Label(self, text="⚙ Serveur RTSP",
                 bg=COLORS["sidebar"], fg=COLORS["blue"],
                 font=("Consolas", 12, "bold")).pack(pady=(14,4))

        self.status_lbl = tk.Label(self, text="● ARRÊTÉ",
                                   bg=COLORS["sidebar"],
                                   fg=COLORS["red"],
                                   font=("Consolas", 11, "bold"))
        self.status_lbl.pack(pady=4)

        # Info fields
        for label, val_fn in [
            ("Hôte",  lambda: self.cfg.host),
            ("Port",  lambda: str(self.cfg.port)),
            ("Canaux",lambda: str(len(self.cfg.channels))),
        ]:
            f = tk.Frame(self, bg=COLORS["sidebar"])
            f.pack(fill="x", padx=12, pady=2)
            tk.Label(f, text=label+":", fg=COLORS["text_dim"],
                     bg=COLORS["sidebar"], font=("Consolas", 9),
                     width=8, anchor="w").pack(side="left")
            lbl = tk.Label(f, text=val_fn(),
                           fg=COLORS["text"], bg=COLORS["sidebar"],
                           font=("Consolas", 9, "bold"))
            lbl.pack(side="left")
            setattr(self, f"_{label.lower()}_lbl", (lbl, val_fn))

        # Time
        self.time_lbl = tk.Label(self, text="",
                                 bg=COLORS["sidebar"], fg=COLORS["text_dim"],
                                 font=("Consolas", 9))
        self.time_lbl.pack(pady=8)
        self._tick()

    def _tick(self):
        self.time_lbl.config(
            text=datetime.datetime.now().strftime("%Y-%m-%d\n%H:%M:%S"))
        # Update dynamic fields
        for attr in ["_hôte_lbl", "_port_lbl", "_canaux_lbl"]:
            if hasattr(self, attr):
                lbl, fn = getattr(self, attr)
                lbl.config(text=fn())
        self.after(1000, self._tick)

    def set_running(self, running: bool):
        if running:
            self.status_lbl.config(text="● EN COURS", fg=COLORS["green"])
        else:
            self.status_lbl.config(text="● ARRÊTÉ", fg=COLORS["red"])


# ────────────────────────────────────────────────
# Main Application
# ────────────────────────────────────────────────
class RTSPManagerApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("RTSP Multi-Channel Server Manager")
        self.geometry("1100x750")
        self.configure(bg=COLORS["bg"])
        self.minsize(900, 600)

        # Config
        cfg_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_CONFIG
        cfg_path.parent.mkdir(parents=True, exist_ok=True)
        self.cfg = ConfigManager(cfg_path)

        # Server process
        self.server_proc = None
        self.server_running = False

        self._build_ui()
        self._start_file_watcher()
        self.log.log(f"Configuration chargée : {cfg_path}", "OK")
        self.log.log("Ajoutez / éditez des canaux, puis lancez le serveur.", "INFO")

    # ── UI ──────────────────────────────────────
    def _build_ui(self):
        # Top toolbar
        toolbar = tk.Frame(self, bg=COLORS["sidebar"], height=48)
        toolbar.pack(fill="x", side="top")
        toolbar.pack_propagate(False)

        tk.Label(toolbar, text="📡  RTSP Manager",
                 bg=COLORS["sidebar"], fg=COLORS["blue"],
                 font=("Consolas", 14, "bold")).pack(side="left", padx=16, pady=8)

        # Toolbar buttons
        for text, color, cmd in [
            ("▶ Démarrer serveur",  COLORS["accent"],  self._start_server),
            ("■ Arrêter serveur",   COLORS["danger"],  self._stop_server),
            ("+ Nouveau canal",     COLORS["accent2"], self._new_channel),
            ("↺ Recharger config",  COLORS["warn"],    self._reload_config),
            ("🗑 Vider journal",    COLORS["border"],  self.log.clear
             if hasattr(self, "log") else lambda: None),
        ]:
            tk.Button(toolbar, text=text, bg=color, fg="white",
                      font=("Consolas", 9, "bold"), relief="flat",
                      padx=10, pady=4, cursor="hand2",
                      command=cmd).pack(side="left", padx=4, pady=8)

        # Main layout
        body = tk.Frame(self, bg=COLORS["bg"])
        body.pack(fill="both", expand=True)

        # Sidebar
        self.status_panel = StatusPanel(body, self.cfg, width=200)
        self.status_panel.pack(side="left", fill="y", padx=(8,0), pady=8)

        # Center panel (channels list)
        center = tk.Frame(body, bg=COLORS["bg"])
        center.pack(side="left", fill="both", expand=True, padx=8, pady=8)

        header = tk.Frame(center, bg=COLORS["bg"])
        header.pack(fill="x", pady=(0,6))
        tk.Label(header, text="📺  Canaux RTSP",
                 bg=COLORS["bg"], fg=COLORS["text"],
                 font=("Consolas", 12, "bold")).pack(side="left")

        # Scrollable channel list
        canvas_frame = tk.Frame(center, bg=COLORS["bg"])
        canvas_frame.pack(fill="both", expand=True)

        self.canvas = tk.Canvas(canvas_frame, bg=COLORS["bg"],
                                highlightthickness=0)
        scrollbar = ttk.Scrollbar(canvas_frame, orient="vertical",
                                  command=self.canvas.yview)
        self.channel_frame = tk.Frame(self.canvas, bg=COLORS["bg"])

        self.channel_frame.bind("<Configure>",
            lambda e: self.canvas.configure(
                scrollregion=self.canvas.bbox("all")))

        self.canvas.create_window((0, 0), window=self.channel_frame,
                                   anchor="nw")
        self.canvas.configure(yscrollcommand=scrollbar.set)
        self.canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        # Log at bottom
        self.log = LogPanel(center)
        self.log.pack(fill="x", pady=(6, 0))

        # Now wire the "vider journal" button
        for w in toolbar.winfo_children():
            if isinstance(w, tk.Button) and "journal" in w.cget("text"):
                w.config(command=self.log.clear)

        self._refresh_channels()

    def _refresh_channels(self):
        for w in self.channel_frame.winfo_children():
            w.destroy()
        for idx, ch in enumerate(self.cfg.channels):
            card = ChannelCard(
                self.channel_frame, ch, idx,
                on_edit=self._edit_channel,
                on_delete=self._delete_channel,
                on_vlc=self._open_vlc,
                on_copy=self._copy_url,
            )
            card.pack(fill="x", pady=(0, 6))

        if not self.cfg.channels:
            tk.Label(self.channel_frame,
                     text="Aucun canal configuré.\nCliquez sur '+ Nouveau canal' pour commencer.",
                     bg=COLORS["bg"], fg=COLORS["text_dim"],
                     font=("Consolas", 10), justify="center").pack(pady=40)

    # ── Actions ─────────────────────────────────
    def _start_server(self):
        if self.server_running:
            self.log.log("Serveur déjà en cours d'exécution.", "WARN")
            return

        exe = Path(__file__).parent / "build" / "rtsp_server"
        if platform.system() == "Windows":
            exe = exe.with_suffix(".exe")
            if not exe.exists():
                exe = Path(__file__).parent / "build" / "Release" / "rtsp_server.exe"

        if not exe.exists():
            self.log.log(
                "Exécutable rtsp_server introuvable. Compilez d'abord le projet C++.", "WARN")
            self.log.log("Simulation du démarrage (mode démo)...", "INFO")
            self.server_running = True
            self.status_panel.set_running(True)
            self._show_stream_urls()
            return

        cfg = str(self.cfg.path)
        try:
            self.server_proc = subprocess.Popen(
                [str(exe), cfg],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True
            )
            self.server_running = True
            self.status_panel.set_running(True)
            self.log.log(f"Serveur démarré (PID {self.server_proc.pid})", "OK")
            self._show_stream_urls()
            threading.Thread(target=self._read_server_log,
                             daemon=True).start()
        except Exception as e:
            self.log.log(f"Erreur démarrage serveur: {e}", "ERR")

    def _read_server_log(self):
        if not self.server_proc: return
        for line in self.server_proc.stdout:
            self.after(0, self.log.log, line.strip(), "INFO")

    def _stop_server(self):
        if self.server_proc:
            self.server_proc.terminate()
            self.server_proc = None
        self.server_running = False
        self.status_panel.set_running(False)
        self.log.log("Serveur arrêté.", "WARN")

    def _show_stream_urls(self):
        for ch in self.cfg.channels:
            if ch.get("enabled"):
                url = self.cfg.rtsp_url(ch["mount"])
                self.log.log(f"  Stream: {url}", "OK")

    def _new_channel(self):
        next_id = len(self.cfg.channels) + 1
        default = {
            "id": next_id, "name": f"Channel_{next_id}",
            "mount": f"/stream{next_id}",
            "source": "videotestsrc", "pattern": (next_id - 1) % 20,
            "text_overlay": f"Canal {next_id} — Live", "enabled": True,
        }
        dlg = ChannelDialog(self, default, "Nouveau canal")
        if dlg.result:
            self.cfg.add_channel(dlg.result)
            self._refresh_channels()
            self.log.log(f"Canal ajouté : {dlg.result['name']}", "OK")

    def _edit_channel(self, idx: int):
        ch = self.cfg.channels[idx]
        dlg = ChannelDialog(self, ch, f"Éditer canal {ch.get('name','')}")
        if dlg.result:
            self.cfg.update_channel(idx, dlg.result)
            self._refresh_channels()
            self.log.log(f"Canal mis à jour : {dlg.result['name']}", "OK")

    def _delete_channel(self, idx: int):
        ch = self.cfg.channels[idx]
        if messagebox.askyesno("Supprimer",
                               f"Supprimer le canal '{ch.get('name')}'?",
                               parent=self):
            self.cfg.remove_channel(idx)
            self._refresh_channels()
            self.log.log(f"Canal supprimé : {ch.get('name')}", "WARN")

    def _open_vlc(self, channel: dict):
        url = self.cfg.rtsp_url(channel["mount"])
        self.log.log(f"Ouverture VLC : {url}", "INFO")
        vlc_exe = self._find_vlc()
        if vlc_exe:
            subprocess.Popen([vlc_exe, url])
        else:
            self.log.log("VLC introuvable. URL copiée dans le presse-papier.", "WARN")
            self.clipboard_clear()
            self.clipboard_append(url)

    def _copy_url(self, channel: dict):
        url = self.cfg.rtsp_url(channel["mount"])
        self.clipboard_clear()
        self.clipboard_append(url)
        self.log.log(f"URL copiée : {url}", "OK")

    def _find_vlc(self):
        candidates = []
        if platform.system() == "Windows":
            candidates = [
                r"C:\Program Files\VideoLAN\VLC\vlc.exe",
                r"C:\Program Files (x86)\VideoLAN\VLC\vlc.exe",
            ]
        elif platform.system() == "Darwin":
            candidates = ["/Applications/VLC.app/Contents/MacOS/VLC"]
        else:
            candidates = ["/usr/bin/vlc", "/usr/local/bin/vlc"]
        for c in candidates:
            if Path(c).exists():
                return c
        # Try PATH
        try:
            result = subprocess.run(["which", "vlc"], capture_output=True, text=True)
            if result.returncode == 0:
                return result.stdout.strip()
        except Exception:
            pass
        return None

    def _reload_config(self):
        self.cfg.load()
        self._refresh_channels()
        self.log.log("Configuration rechargée depuis le fichier.", "OK")

    def _start_file_watcher(self):
        handler = ConfigFileHandler(self.cfg.path, self._on_config_changed)
        self._observer = Observer()
        self._observer.schedule(handler,
                                str(self.cfg.path.parent), recursive=False)
        self._observer.start()

    def _on_config_changed(self):
        self.after(0, self._reload_config)
        self.after(0, self.log.log,
                   "⚡ Fichier config modifié — rechargement automatique", "WARN")

    def on_close(self):
        self._stop_server()
        if hasattr(self, "_observer"):
            self._observer.stop()
            self._observer.join()
        self.destroy()


# ────────────────────────────────────────────────
# Entry point
# ────────────────────────────────────────────────
if __name__ == "__main__":
    app = RTSPManagerApp()
    app.protocol("WM_DELETE_WINDOW", app.on_close)
    style = ttk.Style()
    style.theme_use("clam")
    style.configure("Vertical.TScrollbar",
                    background=COLORS["card"],
                    troughcolor=COLORS["bg"],
                    arrowcolor=COLORS["text_dim"])
    app.mainloop()

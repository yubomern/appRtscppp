# RTSP Multi-Channel Server

Serveur RTSP multi-canal avec rechargement à chaud de la configuration,
incrustation de texte et interface graphique Tkinter.

## Prérequis

### C++ (Serveur RTSP)
- GStreamer >= 1.20 avec MSVC build (Windows) ou via `pkg-config` (Linux/macOS)
- CMake >= 3.16
- Compilateur C++17

### Python (GUI)
```
pip install watchdog
```

## Compilation (Windows)
```cmd
cmake -B build -DGST_ROOT="C:/Program Files/gstreamer/1.0/msvc_x86_64"
cmake --build build --config Release
```

## Compilation (Linux)
```bash
sudo apt install libgstreamer1.0-dev libgstreamer-rtsp-server-1.0-dev
cmake -B build && cmake --build build
```

## Lancement

### Serveur C++
```bash
./build/rtsp_server config/rtsp_config.json
```

### GUI Python
```bash
python rtsp_gui.py config/rtsp_config.json
```

## Flux RTSP
| Canal | URL |
|-------|-----|
| 1 | rtsp://localhost:8554/stream1 |
| 2 | rtsp://localhost:8554/stream2 |
| 3 | rtsp://localhost:8554/stream3 |

## Ouvrir dans VLC
1. Lancer VLC
2. Menu : **Médias > Ouvrir un flux réseau**
3. Coller l'URL RTSP
4. Cliquer Lire

## Modifier la configuration
Éditez `config/rtsp_config.json` — le serveur détecte les modifications
automatiquement et recharge la configuration sans redémarrage.

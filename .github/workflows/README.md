# VocalForge – All-in-One Auto Vocal Chain

Ein VST3-Plugin (plus AU auf dem Mac und Standalone-App), das eine komplette Vocal-Kette
in einem Fenster vereint. Alle Module **analysieren die Stimme laufend und passen sich selbst an** –
deine Regler bestimmen nur, wie stark und in welche Richtung.

## Signalkette

| # | Modul | Was automatisch passiert | Was du einstellst |
|---|-------|--------------------------|-------------------|
| 1 | **Input** | – | Gain, Low Cut (Trittschall/Rumpeln) |
| 2 | **Auto Level** | Fährt die Lautstärke wie ein Vocal-Rider auf einen Zielpegel; in Pausen wird nichts hochgezogen | Ziel-Pegel, max. Regelbereich |
| 3 | **Auto EQ** | Misst die Klangbalance in 6 Bändern und gleicht sie an eine "fertig gemischt"-Kurve an | Stärke, Dunkel/Hell, Body, Präsenz, Air |
| 4 | **Auto De-Esser** | Erkennt Zischlaute relativ zur Stimme (pegelunabhängig), senkt nur das Höhenband ab | Stärke, Frequenz |
| 5 | **Auto Comp** | Threshold folgt automatisch dem Pegel der Stimme, Auto-Make-up | Stärke, Tempo, Parallel-Mix |
| 6 | **Sättigung** | – | Drive (Wärme & Dichte) |
| 7 | **Auto Delay** | Synchron zum DAW-Tempo, Ping-Pong, duckt automatisch während gesungen wird | Notenwert, Feedback, Mix, Ducking |
| 8 | **Auto Reverb** | Gefilterter Hall mit Pre-Delay, duckt während gesungen wird | Größe, Pre-Delay, Mix, Ducking |
| 9 | **Output** | Limiter schützt vor Übersteuerung | Gain, Ceiling, Dry/Wet |

Unten zeigt das Fenster live, was die Automatik gerade tut (EQ-Korrektur je Band, Rider-Gain, Gain Reduction).

**Presets:** Default, Pop Lead, Rap/Hip-Hop, R&B, Rock, Ballade, Podcast, Backing Vocals.
Eigene Presets über *Speichern/Laden* (landen in `Dokumente/VocalForge Presets`).

## Bauen

Du brauchst **CMake (≥ 3.22)**, **Git** und einen C++-Compiler:
- **Windows:** Visual Studio 2022 (Workload "Desktopentwicklung mit C++")
- **macOS:** Xcode (+ `xcode-select --install`)

JUCE wird beim ersten Konfigurieren automatisch geladen.

```bash
cd VocalForge
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Das fertige Plugin wird automatisch in den Standard-Ordner kopiert:
- Windows: `C:\Program Files\Common Files\VST3\VocalForge.vst3` (Terminal ggf. als Administrator starten)
- macOS: `~/Library/Audio/Plug-Ins/VST3/` bzw. `.../Components/` (AU)

Danach in der DAW nach Plugins scannen und VocalForge auf die Vocal-Spur legen.

## Tipps
- Auf eine **trockene, gecomptete-freie Lead-Vocal** legen, Input so einstellen, dass der Input-Meter ca. −18 bis −6 dB zeigt.
- Auto EQ braucht ~1–2 Sekunden Gesang, bis er voll greift.
- Für Rap/Doubles Hall- und Delay-Mix runter, für Balladen hoch.
- Wenn du Delay/Hall lieber auf Send-Spuren hast: Module einfach ausschalten.

## Offline-Test (optional)
`cmake -B build -DVF_TESTS=ON && cmake --build build --target VocalForgeTest` – schickt ein synthetisches
Gesangssignal durch alle Module und prüft Stabilität, Pegelausgleich, De-Essing, Presets und State-Speicherung.

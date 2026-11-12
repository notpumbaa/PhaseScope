# PhaseScope

Analyse-Plugin (VST3 / AU / Standalone) zur Visualisierung von Phasenproblemen im Mastering. Gebaut mit JUCE 8.

## Anzeigen

| Bereich | Was es zeigt |
|---|---|
| **Goniometer** | Lissajous-Darstellung (M senkrecht, S waagerecht) mit Nachleuchten und Auto-Gain. Senkrechter Strich = mono, Kreis/Fläche = breites Stereo, waagerechte Ausdehnung = gegenphasige Anteile. |
| **Korrelation Vollband** | Korrelationsgrad −1 … +1 mit Min-Hold. Rot = negativ (Auslöschung bei Mono-Summe). |
| **Korrelation pro Band** | Bass < 150 Hz, Mitten 150 Hz – 2,5 kHz, Höhen > 2,5 kHz (Linkwitz-Riley-Weiche). Zeigt, *wo* das Problem sitzt – z. B. phasengedrehter Bass bei sonst gesundem Mix. |
| **Mono-Verlust** | Pegel der Mono-Summe gegenüber dem Stereo-Pegel in dB, mit Max-Hold. 0 dB = mono, ca. −3 dB = unkorreliert, deutlich darunter = Auslöschung. |
| **Verlauf** | Korrelation Vollband und Bass über die letzten 10 Sekunden. |

## Bedienung

- **Integration** (50–3000 ms, Standard 300 ms): Trägheit der Korrelationsmessung.
- **Mono-Abhöre**: summiert den Ausgang auf Mono zum Gegenhören. Sonst wird das Audio unverändert durchgereicht.
- **Hold zurücksetzen**: setzt Min-/Max-Werte zurück (z. B. vor einem neuen Durchlauf des Songs).

## Bauen

Voraussetzungen: CMake ≥ 3.22, ein C++17-Compiler (Visual Studio 2022 unter Windows, Xcode unter macOS) und Git. JUCE wird beim ersten Konfigurieren automatisch heruntergeladen.

**Windows**
```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```
Plugin: `build\PhaseScope_artefacts\Release\VST3\PhaseScope.vst3` → nach `C:\Program Files\Common Files\VST3\` kopieren.

**macOS**
```
cmake -B build -G Xcode
cmake --build build --config Release
```
VST3 → `~/Library/Audio/Plug-Ins/VST3/`, AU (`.component`) → `~/Library/Audio/Plug-Ins/Components/`.

**Linux** (benötigt u. a. `libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype-dev libfontconfig1-dev`)
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Eine lokale JUCE-Kopie lässt sich mit `-DJUCE_DIR=/pfad/zu/JUCE` verwenden.

## Einsatz

Als letztes Plugin auf den Master-Bus (nach Limiter), damit das gemessen wird, was tatsächlich ausgespielt wird.

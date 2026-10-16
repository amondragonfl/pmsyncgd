# pmsyncgd ♪

Unlocks the **Practice Music Sync** toggle in Geometry Dash (Steam, Windows), so practice mode
plays the level's real song, in sync with where you are, without owning the **Music Unlocker**
shop item.

```
     ┌───────┐
     │ ■   ■ │   ♪ pmsyncgd ♫
     │ \___/ │   practice music sync, unlocked
     └───────┘   for Geometry Dash on Steam

  ♪ step 5/5 · picking the locks...
      ♪ lock 1/5 open!  the toggle in the options menu @ GeometryDash.exe+0x2c207b
      ♪ lock 2/5 open!  reading the toggle @ GeometryDash.exe+0x2c4695
      ♪ lock 3/5 open!  flipping the toggle @ GeometryDash.exe+0x2c49db
      ♪ lock 4/5 open!  the "you need a Music Unlocker" popup @ GeometryDash.exe+0x2c4a68
      ♪ lock 5/5 open!  music sync inside levels @ GeometryDash.exe+0x50da85
      √ double-checked from disk: 5/5 locks open

  \(^o^)/  practice music sync is unlocked!
```

## Using it

1. Close Geometry Dash.
2. Double-click **`pmsyncgd.exe`** and pick **`[1] install`**. It finds the game by itself.
3. Open GD, start any level, pause it, open the **options** (the gear in the pause menu), and
   turn on **Practice Music Sync**. It may be on a later page.
4. Enter practice mode and enjoy the real song. ♫

When Steam updates Geometry Dash, the update replaces the game file and the mod goes away.
Run pmsyncgd again after an update.

### Modes

| Menu / command | What it does |
|---|---|
| `install` | Patches `GeometryDash.exe` so the change persists. Saves `GeometryDash.exe.pmsyncgd-backup` next to it first. |
| `uninstall` | Puts the original code back and checks the result against the backup. |
| `live` | Patches the **running** game in memory only. No files change, and the patch lasts until GD closes. Start pmsyncgd before or after launching GD; it waits and watches. |
| `status` | Shows the game folder, Steam build, whether the mod is installed, and whether GD is running. |

```
pmsyncgd [install | uninstall | live | status] [--path "<GD folder>"] [--once] [--fast]
```

* `--path`: use this if pmsyncgd can't find your game. You can also drop `pmsyncgd.exe`
  into the Geometry Dash folder and run it from there.
* `--once`: in live mode, patch once and exit instead of watching for restarts.
* `--fast`: skip the dramatic pauses.

### Is it safe?

* It changes **5 tiny spots of 5 bytes each**, and only the spots that check for the Music
  Unlocker. The shop, your save file, and everything else are untouched.
* The original game file is backed up before anything changes. Uninstall restores it byte for
  byte.
* Steam can always restore the game: right-click Geometry Dash › Properties › Installed Files ›
  **Verify integrity of game files**.
* Admin rights usually aren't needed, because Steam's game folder is normally writable by
  your user. If Windows says no, pmsyncgd tells you to run it as administrator.
* If your GD version doesn't match what pmsyncgd expects, it refuses to touch anything.

## How it works

The toggle is real and fully implemented in the game. It's only gated behind
`GameStatsManager::isItemUnlocked(GJItem, 17)`, where item 17 is the Music Unlocker.
pmsyncgd finds every place that asks that exact question and swaps the `call` for
`mov al, 1`, so the answer is always "yes". It locates them by byte signature, not hard-coded
addresses, so it has a good chance of surviving small GD updates.

The full reverse-engineering notes are in **[docs/how-it-works.md](docs/how-it-works.md)**.

## Building

You need [MinGW-w64](https://www.mingw-w64.org/) (x86_64 `gcc` and `windres` on `PATH`).

```bat
build.bat                 :: -> dist\pmsyncgd.exe  (~70 KB, no DLLs needed)
powershell -ExecutionPolicy Bypass -File tests\test.ps1
```

The tests run live mode against a fake `GeometryDash.exe` (`tests/fake_gd.c`) and do an
install/uninstall round trip on a **temporary copy** of your real game file. Your actual
install is never touched.

`tools/make_icon.py` regenerates the little cube icon in `res/` (Python stdlib only).

## Compatibility

Made and tested on Geometry Dash for Windows, **Steam build 25909607** (October 2026).

## Disclaimer

A fan-made mod, not affiliated with or endorsed by RobTop Games. It only affects a
single-player practice feature, but you use it at your own risk.

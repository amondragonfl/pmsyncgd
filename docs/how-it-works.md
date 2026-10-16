# How pmsyncgd works

These are the reverse-engineering notes behind the mod. All addresses are RVAs in
`GeometryDash.exe` for **Steam build 25909607** (x64, image base `0x140000000`).
Class and function names come from what the code does and from community naming. GD ships
without symbols, so treat the names as educated labels.

## 1. Finding the feature

The binary contains these strings:

| RVA | String |
|---|---|
| `0x7608d0` | `Practice Music Sync` |
| `0x7608f0` | `Plays normal music in practice mode in main and online levels. <cy>Requires the Music Unlocker.</c>` |
| `0x761140` | `You need a <cg>Music Unlocker</c> to play <cy>normal music</c> in <cl>practice mode</c>.` |
| `0x76a970` | `Music Unlocker` |

Following the RIP-relative references to those strings leads to the code below.

## 2. The gate

Every check goes through one function:

```cpp
bool GameStatsManager::isItemUnlocked(UnlockType type, int id);   // 0x2e6b30
// UnlockType::GJItem == 12 (0x0C), Music Unlocker == item 17 (0x11)
```

There are five places that ask it about the Music Unlocker.

**Options popup** (`0x2c09d0`, the pause menu's gameplay options with Auto-Retry, Faster
Reset, Progress Bar, and so on):

```cpp
addToggle("Practice Music Sync", /*id*/ -2,
          /*locked*/ !gsm->isItemUnlocked(GJItem, 17),        // call @ 0x2c207b, then xor al,1
          "Plays normal music in practice mode ...");
```

**Toggle value getter** (`0x2c4490`, case `-2`):

```cpp
if (!gsm->isItemUnlocked(GJItem, 17)) return false;            // call @ 0x2c4695
return gsm->m_enabledItems->valueForKey(itemKey(17, GJItem))->boolValue();
```

**Toggle handler** (`0x2c47d0`, case `-2`):

```cpp
bool current = gsm->isItemUnlocked(GJItem, 17) && enabled(17); // call @ 0x2c49db
if (!gsm->isItemUnlocked(GJItem, 17)) {                         // call @ 0x2c4a68
    setToggle(-2, current);        // snap the checkbox back
    showLockedPopup();             // 0x2c5b20 -> "You need a Music Unlocker ..."
    return;
}
if (current != wanted)
    gsm->toggleEnableItem(GJItem, 17, wanted);                  // 0x2e6ed0
```

**Level setup** (`0x50da20`, in the play layer):

```cpp
m_practiceMusicSync =                                          // byte at this+0x3321
    (gsm->isItemUnlocked(GJItem, 17) && enabled(17))           // call @ 0x50da85
    || (GameManager::getGameVariable("0125") && m_level->m_levelType == 2 /* local level */);
```

`toggleEnableItem` itself doesn't check ownership. It stores `"1"` under the item's key in
`GameStatsManager+0x380`, and that gets saved with the rest of your stats. So the feature
works end to end, and `isItemUnlocked(GJItem, 17)` is its only barrier.

## 3. The patch

All five call sites have the same shape:

```
BA 0C 00 00 00        mov  edx, 0Ch          ; UnlockType::GJItem
41 B8 11 00 00 00     mov  r8d, 11h          ; Music Unlocker
E8 xx xx xx xx        call isItemUnlocked
84 C0  |  34 01       test al, al  |  xor al, 1
```

pmsyncgd replaces the 5-byte `call` with:

```
B0 01                 mov  al, 1
90 90 90              nop ; nop ; nop
```

The callers only look at `al` afterwards, and the call's volatile registers aren't relied on
after a call, so skipping it is safe. Nothing else in the game changes:

* The shop still asks `isItemUnlocked` through code with a *computed* item id, so it is
  unaffected. The Music Unlocker still shows up for sale.
* The menu-music customizer (items 6–15) and every other item check are untouched.

### The decoy

The same `mov edx, 0Ch / mov r8d, 11h / call` prefix also appears at `0x2c4adc`, but that
call goes to **`toggleEnableItem`**, which saves the toggle state. Patching it would break
saving. pmsyncgd skips it for two independent reasons: it doesn't call `isItemUnlocked`, and
it isn't followed by a `test al,al` / `xor al,1`.

### Finding `isItemUnlocked` without hard-coding it

pmsyncgd doesn't store `0x2e6b30`. Instead, every `mov edx,0Ch / mov r8d,<any id> / call X`
that's followed by a bool check casts a vote for `X`. `isItemUnlocked` wins by a landslide:
14 votes in this build, and nothing else gets any. It still has 9 once the Music Unlocker
sites are patched. That's also how `uninstall`
rebuilds the original `call` bytes, even when every Music Unlocker site is already patched,
because the other items' checks still point at the real function.

## 4. Install vs. live

* **install** writes the five 5-byte patches into `GeometryDash.exe` on disk, after copying
  the untouched file to `GeometryDash.exe.pmsyncgd-backup`. The exe has no Authenticode
  signature and a zero PE checksum, so nothing else needs fixing up. Steam's game folder
  grants `Users` full control, so no elevation is needed.
* **live** opens the running `GeometryDash.exe` process, reads `.text` from memory, runs the
  same scanner, and writes the same bytes with `VirtualProtectEx`/`WriteProcessMemory`. When
  the game closes, the patch is gone.

## 5. When GD updates

Run `pmsyncgd status`.

* If it still finds **5 locks**, the signature held and `install` will work.
* If it finds **0**, the code shape changed. Look for new references to the strings in
  section 1 and update `k_prefix`, `uses_bool`, or the patch in `src/patcher.c` to match.
  The friendly labels in `src/main.c` (`k_known`) are keyed by RVA and only cover this build.
  On other builds the locks show as "a Music Unlocker check", which is harmless.

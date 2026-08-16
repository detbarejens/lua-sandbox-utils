# HZ

FiveM external. Engine and features come from `E:\BH\c\FC2` plus the FlatScreen menu layout at `E:\FS COpy`. New brand, login hub, and key redeem live here.

## Builds

| Script | Output | Who |
|---|---|---|
| `Build-Dev.bat` | `Client\HZ-Dev.exe` | You. No login. Console on. Dev tools on. |
| `Build-Retail.bat` | `Client\HZ-Retail.exe` | Friends. Login → waiting hub → overlay. |

## Retail flow

1. Open `HZ-Retail.exe`
2. Redeem an `HZ1....` key
3. Hub waits until FiveM is in-game
4. Overlay attaches. Menu key is the same bind as before (Insert by default unless you changed it)

## Keys

```bat
cd F:\project\tools\KeyGen
FriendsKeyGen.bat init
FriendsKeyGen.bat key --days 30 --note "name"
FriendsKeyGen.bat list
FriendsKeyGen.bat revoke 1
```

`init` and `revoke` rewrite `HZ\Definations\FriendsLicensePub.hpp`. Rebuild retail after those.

Private key: `tools\KeyGen\friends-private.bin` — never share, never commit.

## Stream / NVIDIA capture

The overlay and hub call `SetWindowDisplayAffinity(..., WDA_EXCLUDEFROMCAPTURE)` so Discord / OBS / NVIDIA overlay skip the window. That is the Windows capture-exclusion API, not a patch of `nvcontainer.exe`.

## Notes

- `E:\FS` as a drive path does not exist. Functions were taken from `E:\FS COpy` (FlatScreen) and `E:\BH\c\FC2`.
- HZ keys (`HZ1.`) are a different keyspace from Trinity `TRN1.` friends keys.

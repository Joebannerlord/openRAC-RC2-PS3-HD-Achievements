# RAC2 HD Achievements

Track all 35 Going Commando HD trophies in OpenRAC. Load an existing save to credit completed trophies the game can prove. New unlocks show a tier-colored popup with an optional chime. Progress stays local.

Supports **OpenRAC 1.1.0 on Windows and Linux x86-64** with **Ratchet & Clank: Going Commando USA v1.01** (`SCUS-97268`). The plugin uses OpenRAC mod API 1.14.

## Install

1. Select **Going Commando** in OpenRAC Launcher.
2. Open **Mods → Add from a link** and paste this repository address.

   ```text
   https://github.com/Joebannerlord/openRAC-RC2-PS3-HD-Achievements
   ```

3. Choose **Install**, enable the mod, and start the game.

You can also download the ZIP for your platform and import it through **Mods → Add a mod**. [Releases](https://github.com/Joebannerlord/openRAC-RC2-PS3-HD-Achievements/releases) · [OpenRAC installation guide](https://openrac.dev/mods/docs/install-from-link)

## Trophy collection

Open **Mods → RAC2 HD Achievements → Open** for trophy requirements, earned status, and progress. After loading a save, use **Refresh** if the page is already open. Earned cards have animated tier-colored borders that respect your reduced-motion preference. The release uses built-in tier medals.

Press **Insert** during play to preview **More Vandalism?** without advancing trophy progress. The preview key and chime are configurable in the mod settings.

Bolt trophies count cumulative earnings per campaign, including Challenge Mode. The mod credits historical earnings that the saved wallet, counted pickups, and certain stock purchases can prove. Spending that left no record cannot be recovered. A direct payout can be missed if spending masks it in the same tick.

## Build

On Windows, use an x64 Visual Studio developer prompt with MSVC. On Linux x86-64, use GCC or Clang. Both need CMake 3.20 or newer and Python 3. The fetcher verifies the OpenRAC Mod SDK pinned in [sdk.lock](sdk.lock).

```powershell
python tools/fetch_sdk.py
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target release_zip
```

The platform ZIP is written to `build/dist/`. The included tests use simulated inputs and the public SDK.

On Linux, popup text uses the game overlay font and the built-in tier medals.

## Credits and license

- **Joebannerlord** maintains the mod and verified trophy behavior in play. **Codex** assisted with development and verification.
- The trophy collection and popup are based on RAC1 HD Achievements v0.5.2. [RAC1 release](https://github.com/Joebannerlord/openRAC-RC1-PS3-HD-Achievements/releases/tag/v0.5.2)
- **OpenRAC contributors** provide the mod SDK, build template, and Going Commando save parser. [OpenRAC](https://github.com/OpenRAC/OpenRAC) · [SDK MIT notice](LICENSE-OPENRAC-SDK)

RAC2 HD Achievements is licensed under GPL-3.0-or-later. The bundled font is Audiowide. [Mod license](LICENSE) · [Audiowide license](ui/LICENSE-Audiowide.txt)

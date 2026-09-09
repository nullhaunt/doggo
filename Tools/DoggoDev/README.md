# DOGGO Dev

`doggo-dev` is the first host-side development loop for DOGGO. It configures and builds the game, uploads the NRO
with `nxlink`, launches it through hbmenu's netloader, and leaves nxlink's log server attached to the process.

## Build the tool

```sh
cmake -S Tools/DoggoDev -B build/doggo-dev -DCMAKE_BUILD_TYPE=Release
cmake --build build/doggo-dev --config Release
```

## Run on a Switch

1. Put the PC and Switch on the same network.
2. Open hbmenu on the Switch and start netloader (normally with `Y`).
3. From the DOGGO repository root, run:

```sh
build/doggo-dev/doggo-dev run --switch 192.168.1.50
```

With a multi-configuration Windows generator, the executable is normally
`build/doggo-dev/Release/doggo-dev.exe` instead.

The command uses the `doggo-debug` and `game-debug` presets, uploads
`build/doggo-debug/Game/doggo_game.nro`, and prints the live DOGGO log until the app exits. Use `--config release`
for the release presets or `--no-build --nro <path>` to deploy an existing NRO.

`nxlink` is resolved from `--nxlink`, `DOGGO_NXLINK`, `$DEVKITPRO/tools/bin`, or `PATH`, in that order. The Switch
address can also be set once as `DOGGO_SWITCH_IP`. The host must allow inbound TCP connections to nxlink's callback
port (28771). WSL2 users should use mirrored networking or run the tool directly on the LAN-facing host.

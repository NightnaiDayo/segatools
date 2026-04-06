# Taisen configuration settings

This file describes configuration settings specific to the Taisen series.

Keyboard binding settings use
[Virtual-Key Codes](https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes).

## `[ektio]` / `[sekitoio]`

Controls the input driver.

### `path`

Specify a path for a third-party input driver DLL. Default is empty
(use built-in keyboard IO emulation).

## `[flatPanelReader]`

Controls settings for the Flat Panel (a.k.a. Y3 Board)

### `enable`

Default: `1`

Whether or not to enable Flat Panel emulation. Disable to use a real board.

### `port_field`

Default: `10`

The COM port on which the emulated Flat Panel is connected to. This differs per game.

### `port_printer`

Default: `11`

The COM port on which the emulated Printer Camera is connected to. This  only exists in Sangokushi.

### `dllVersion`

Default: `1`

The version of the emulated Y3CodeReader dll. Unsure if that is used anywhere.

### `firmVersion`

Default: `1`

The version of the emulated Y3CodeReader firmware. Unsure if that is used anywhere.

### `firmNameField`

Default: `SFPR`

The device name of the emulated Flat Panel. This should never need changing.

### `firmNamePrinter`

Default: `SPRT`

The device name of the emulated Printer Camera. This should never need changing.

### `targetCodeField`

Default: `SFR0`

The target name of the emulated Flat Panel. This should never need changing.

### `targetCodePrinter`

Default: `SPT0`

The target name of the emulated Printer Camera. This should never need changing.

## `[y3ws]`

Settings for the default implementation of setting cards on the Flat Panel remotely via websockets

### `enable`

Default: `1`

Enable the websocket server.

### `debug`

Default: `0`

Makes y3ws I/O very verbose. For debugging only. May lag the game.

### `port`

Default: `3594`

The TCP port the websocket server listens on.

### `gameId`

Default: `SDEY`

The game ID that the websocket server transmits, so clients can change their behaviour based on that (UI, etc.)

### `cardDirectory`

Default: `DEVICE\print`

The directory where printed card images are placed. Should be the same as in `[printer]`.
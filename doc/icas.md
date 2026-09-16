# ICAS reader emulation

The ICAS hook lets segatools provide card input to ThincaPayment, the game
component that handles e-money payments. You can present a card by holding a
key to read its ID from a file, or by tapping an AimeIO reader. Tested in SDHD.

## Setup

1. Set up terminal authentication as described in [the e-money guide](emoney.md).
1. Build segatools using the [development instructions](development.md) and install
   the rebuilt hook in the 64-bit `amdaemon.exe` process.
1. Use ThincaPayment 1.4.11 and TFPS reader adapter 1.0.3
   (`aime_rw_adapterMD.dll`).
1. Add the following settings to `segatools.ini`, merging any sections that
   already exist:

```ini
[epay]
enable=1
hook=0

[icas]
enable=1
aimeio=0
felicaPath=DEVICE\felica.txt

[aime]
scan=0x0D
```

Create `DEVICE\felica.txt` containing a 16-digit hexadecimal FeliCa IDm:

```text
0123456789ABCDEF
```

Select a payment brand in the game and hold Enter when prompted to scan a card.
The scan key is configured with `[aime] scan`. The card file is read on each scan;
spaces and line endings are allowed. Paths are relative to the daemon's working
directory.

### AimeIO reader

Install the 64-bit AimeIO plugin for your reader and configure its path. For
HINATA, see the [AimeIO setup instructions](https://hinata.neri.moe/en/game-setting/sega/manual/aimeio/).

```ini
[aime]
enable=1

[aimeio]
path=hinata.dll

[epay]
enable=1
hook=0

[icas]
enable=1
aimeio=1
```

Tap the card when the payment screen requests it. ICAS shares the primary AimeIO
reader with normal login. This mode uses the plugin's FeliCa IDm instead of the
scan key and card file. Use AimeIO mode for the reader; direct serial mode with
`[aime] enable=0` bypasses the shared backend.

### Aime access codes

Set `aimeAccessCode=1` to use a 20-digit Aime access code as the payment
identity:

```ini
[icas]
aimeAccessCode=1

[aime]
aimePath=DEVICE\aime.txt
```

With `aimeio=0`, hold the scan key to read the code from `aimePath`. With
`aimeio=1`, tap the reader to use the plugin's `aime_io_nfc_get_aime_id` output.
`felicaPath` is ignored in this mode. The file accepts spaces and line endings;
leading zeroes are preserved.

ThincaPayment expects an eight-byte card ID. Segatools converts the access code
into that format by hashing its 20 digits with SHA-256 and taking the first
eight bytes:

```text
00123456789012345678 -> 3dea873e45c05c48
```

Amusement IC codes contain a three-digit company code, 12 digits of encoded card
data, and a five-digit checksum. Hashing the whole code keeps cards from different
companies distinct and lets the result use all 64 bits of the payment ID. The
checksum is calculated from the other digits, so it adds no new randomness. The
same access code produces the same payment ID whether read from a file or AimeIO.

Enabling this setting changes the payment ID and its server wallet. Existing
balances remain associated with the previous ID.

## Configuration

Settings are in `[icas]` unless otherwise specified.

| Setting | Default | Description |
| --- | --- | --- |
| `[epay] enable` | `1` | Set to `1` to enable e-money support |
| `[epay] hook` | `1` | Set to `0` to use the real ThincaPayment client |
| `enable` | `0` | Enable ICAS reader emulation |
| `aimeio` | `0` | Use the shared AimeIO reader |
| `aimeAccessCode` | `0` | Derive the payment ID from an Aime access code |
| `felicaPath` | `[aime] felicaPath` or `DEVICE\felica.txt` | FeliCa IDm file |
| `[aime] aimePath` | `DEVICE\aime.txt` | Aime access-code file |
| `[aime] scan` | `0x0D` | Scan key; defaults to Enter |

## Troubleshooting

* Initialization fails: check the client and adapter versions, the 64-bit daemon
  hook, and `hook=0`.
* Enter does nothing: hold the scan key while the payment screen requests a card.
* Invalid card file: check the working directory and file path. Use 16 hexadecimal
  digits for FeliCa or 20 decimal digits for an Aime access code.
* Reader times out: check normal Aime login, the plugin architecture, and the
  identity function selected by `aimeAccessCode`.
* `AimeIO requires a rebuilt Segatools hook`: install the normal hook with the
  shared-reader exports in the 64-bit daemon process.

Look for `ICAS: Emulation enabled`, `Reader claimed`, and `Card tapped` in the
hook output. Status `0x20002` means timeout; `0x20003` means cancelled. Set
`emoney.log.level` to `4` in the daemon configuration for payment-client logs.

## Technical info

ThincaPayment asks a reader adapter to open the reader, wait for a card, and
return its identity. The ICAS hook supplies an emulated adapter for those calls.
It handles opening and closing the reader, card detection, timeouts,
cancellation, and basic FeliCa polling. ThincaPayment and the payment server
handle balances and settlement.

A detected FeliCa card has two eight-byte values: its IDm (card identifier) and
PMm (card parameters). Segatools takes the IDm from the configured input and
returns a fixed PMm of `00 F1 00 00 00 01 43 00`. See
[the TCAP operation layout](emoney.md#entity-operations) for the server messages
that ThincaPayment translates into reader calls.

The generic NFC path starts with `OPEN_RW`, which selects reader 0 and supplies
card-type mask `0009`. The FeliCa bit in that mask is `0008`, so successful
FeliCa detection reports card type 8. The adapter writes the IDm and PMm into
separate output buffers supplied by the client. ThincaPayment then packages
those values into the TCAP response sent to the server.

The direct FeliCa path accepts the six-byte Polling request
`06 00 SS SS RR TT`:

| Bytes | Meaning |
| --- | --- |
| `06` | Total request length, including this byte |
| `00` | Polling command |
| `SS SS` | Two-byte system code |
| `RR` | Request code: `00` for card identity, `01` to also return the system code |
| `TT` | Time slot |

For request code `00`, the response is 18 bytes: one length byte, response code
`01`, the eight-byte IDm, and the eight-byte PMm. Request code `01` appends the
two system-code bytes from the request, making the response 20 bytes. Other
commands return device-error status `0x20001`.

Reader calls return adapter status codes. These are distinct from the Windows
HRESULT values used by the AimeIO bridge:

| Status | Meaning |
| --- | --- |
| `0` | Success; requested outputs have been written |
| `0x10001` | Invalid argument |
| `0x10002` | The required reader mode is not open |
| `0x20001` | Device error or unsupported operation |
| `0x20002` | Card detection timed out |
| `0x20003` | Operation cancelled |

AimeIO input reuses the primary reader already initialized by segatools. The
normal reader lock keeps payment polling and login polling from accessing the
plugin at the same time. For FeliCa, the plugin returns the IDm as a number;
segatools sends its most significant byte first. For Aime, the plugin returns
ten packed-BCD bytes, with two decimal digits in each byte. Segatools converts
these to 20 ASCII digits before applying the access-code mapping described above.

The reader adapter is implemented in
[icasrw.c](../common/platform/icasrw.c), the shared AimeIO bridge in
[sg-reader.c](../common/board/sg-reader.c). Both are built into the normal game
hook. Standalone tests are in `tests/icasrw-*.c`; their headers contain build
instructions.

### Implementation findings

* **Terminal authentication and payment use different reader paths.** Terminal
  authentication reads the setup card through the emulated Aime reader, as
  described in the e-money guide. Payment card detection uses the Thinca reader
  adapter in 64-bit amdaemon. The 32-bit game reads the same INI file but skips
  ICAS initialization.
* **There are two ways for the client to obtain a reader.** It can call
  `CreateThincaRwAdapter`, or amdaemon can pass it a built-in reader directly
  through `setRwAdapter`. ICAS intercepts both. The factory can be looked up by
  its name or export number (ordinal) 2. In ThincaPayment 1.4.11, `setRwAdapter`
  is entry 8 in the virtual method table, at offset `0x4e200` from the loaded
  DLL's base address. The hook checks the version and address before replacing
  that entry.
* **Startup can replace the factory hook.** Later platform initialization can
  overwrite the patched `GetProcAddress` entry used to find the reader factory.
  ICAS restores that hook immediately before calling the original payment
  initializer, so it is active when the client needs it.
* **Replacing a reader does not transfer ownership of the original.** Amdaemon
  may keep its built-in reader and register it again, so ICAS leaves that object
  intact. When registration succeeds, the payment client takes ownership of the
  new emulated reader. If registration fails, ICAS frees only the reader it just
  allocated.
* **Payment scans must share the existing AimeIO instance.** Polling and reading
  the card ID happen together under the normal reader lock. If the backend has
  not started, the bridge returns `E_PENDING` and ICAS retries until the scan
  times out. A missing card or reader error does not switch input to a saved
  card file.
* **The bytes after the IDm are a length, not part of the card ID.** In the
  client's 27-byte detection payload, the eight IDm bytes begin at offset 9.
  The next two bytes, `00 08`, give the PMm length. Including them in the card
  ID produces 20 hexadecimal characters instead of the correct 16. The adapter
  itself returns IDm and PMm separately; the client adds this framing.
* **File and AimeIO access codes must use exactly the same hash input.** Both
  paths hash 20 ASCII digits, including leading zeroes, with no whitespace or
  terminating null byte. This gives the same payment ID for the same code and
  retains the company code as part of the input.

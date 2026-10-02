# Thinca payment reader emulation

The ICAS hook provides card input to ThincaPayment, the game component that
handles e-money payments. Present a card by holding a key to read its ID from a
file, or by tapping an AimeIO reader. Tested with two games.

## Setup

1. Set up terminal authentication as described in [the e-money guide](emoney.md).
1. Use a segatools build that includes ICAS. To build it yourself, follow the
   [development instructions](development.md). The game's launch script
   injects the hook into the 64-bit `amdaemon.exe` process, where payments run.
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
The scan key is configured with `[aime] scan`. The card file is read on each
scan; spaces and line endings are allowed. Paths are relative to the daemon's
working directory.

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
reader with normal login and uses the plugin's FeliCa IDm instead of the scan key
and card file. Keep `[aime] enable=1`: the shared reader is part of segatools'
Aime reader emulation.

### Aime access codes

The Thinca reader interface takes a FeliCa IDm: an eight-byte card identifier,
written as 16 hexadecimal digits. An Aime access code is 20 decimal digits, so
segatools converts it into an IDm.

To pay with the same access code you use for game login, set
`aimeAccessCode=1`. Segatools converts the access code into an eight-byte value
and presents it to Thinca as the card's FeliCa IDm. No separate FeliCa IDm is
needed:

```ini
[icas]
aimeAccessCode=1

[aime]
aimePath=DEVICE\aime.txt
```

With `aimeio=0`, hold the scan key to read the code from `aimePath`. The file
accepts spaces and line endings, and leading zeroes are kept. With `aimeio=1`,
tap the reader to use the plugin's `aime_io_nfc_get_aime_id` output. Cards that
the plugin reports only by FeliCa IDm, such as most Amusement IC cards, belong in
`aimeAccessCode=0` mode. `felicaPath` is not used in this mode.

The conversion hashes the 20 digits with SHA-256 and takes the first eight
bytes. The same access code always produces the same IDm, whether it is read
from a file or from AimeIO:

```text
00123456789012345678 -> 3dea873e45c05c48
```

Amusement IC codes contain a three-digit company code, 12 digits of encoded card
data, and a five-digit checksum. Hashing the whole code includes the company
code and uses all 64 bits of the IDm. The checksum is calculated from the other
digits, so it adds no randomness.

## Configuration

Settings are in `[icas]` unless otherwise specified.

| Setting | Default | Description |
| --- | --- | --- |
| `[epay] enable` | `1` | Set to `1` to enable e-money support |
| `[epay] hook` | `1` | Set to `0` to use the real ThincaPayment client; required by ICAS |
| `enable` | `0` | Enable ICAS reader emulation |
| `aimeio` | `0` | Use the shared AimeIO reader |
| `aimeAccessCode` | `0` | Derive the payment ID from an Aime access code |
| `felicaPath` | `[aime] felicaPath` or `DEVICE\felica.txt` | FeliCa IDm file |
| `[aime] aimePath` | `DEVICE\aime.txt` | Aime access-code file |
| `[aime] scan` | `0x0D` | Scan key; defaults to Enter |

## Troubleshooting

On startup, amdaemon's hook output shows `ICAS: Emulation enabled` with the
configured input source. During a payment it shows `Reader claimed`, then
`Card tapped`. A game with a 64-bit game process prints the startup line twice,
once for each process.

If ICAS cannot start, the game still boots without card emulation and the hook
output names the cause:

| Message | Fix |
| --- | --- |
| `[epay] hook=1 replaces ThincaPayment` | Set `[epay] hook=0` |
| `ThincaPayment.dll is not loaded` | Run the daemon from the directory containing the payment DLLs |
| `aime_rw_adapterMD.dll is not loaded` | Place the TFPS reader adapter beside `ThincaPayment.dll` |
| `Unsupported ThincaPayment version` | Use ThincaPayment 1.4.11 |
| `Invalid [aime] scan key` | Set `[aime] scan` to a virtual-key code from `0x01` to `0xFF` |

During a payment:

| Symptom | Fix |
| --- | --- |
| Enter does nothing | Hold the scan key while the payment screen requests a card |
| `Invalid or missing card file` | Check the path and working directory. Use 16 hexadecimal digits for FeliCa or 20 decimal digits for an access code |
| Reader times out | Check that the card works for normal Aime login, the plugin is 64-bit, and `aimeAccessCode` matches the card type |
| `AimeIO requires a Segatools hook with the ICAS bridge` | Use a segatools build that includes ICAS |

Set `emoney.log.level` to `4` in the daemon configuration for payment-client
logs. Reader status codes in those logs are listed under
[Technical info](#technical-info).

## Technical info

ThincaPayment asks a reader adapter to open the reader, wait for a card, and
return its identity. The ICAS hook supplies an emulated adapter for those calls.
It handles opening and closing the reader, card detection, timeouts,
cancellation, and FeliCa Polling.

A detected FeliCa card has two eight-byte values: its IDm (card identifier) and
PMm (card parameters). Segatools takes the IDm from the configured input and
returns the Amusement IC PMm `00 F1 00 00 00 01 43 00`, the same value used by
the normal Aime reader emulation. See
[the TCAP operation layout](emoney.md#entity-operations) for the server messages
that ThincaPayment translates into reader calls.

The generic NFC path starts with `OPEN_RW`, which selects reader 0 and supplies
card-type mask `0009`. The FeliCa bit in that mask is `0008`, so successful
FeliCa detection reports card type 8. The adapter writes the IDm and PMm into
separate output buffers supplied by the client. ThincaPayment then packages
those values into the TCAP response sent to the server.

The direct FeliCa path handles the six-byte Polling request
`06 00 SS SS RR TT`:

| Bytes | Meaning |
| --- | --- |
| `06` | Total request length, including this byte |
| `00` | Polling command |
| `SS SS` | Two-byte system code; `FF FF` matches any card |
| `RR` | Request code: `00` for card identity, `01` to also return the system code |
| `TT` | Time slot |

For request code `00`, the response is 18 bytes: one length byte, response code
`01`, the eight-byte IDm, and the eight-byte PMm. Request code `01` appends a
two-byte system code, making the response 20 bytes. A specific system code is
returned as requested. The wildcard `FF FF` returns `88 B4`, the Amusement IC
system code. Other FeliCa commands return status `0x20001`.

Reader calls return adapter status codes, matching the real adapter. These are
distinct from the Windows HRESULT values used by the AimeIO bridge:

| Status | Meaning |
| --- | --- |
| `0` | Success; requested outputs have been written |
| `0x10001` | Invalid argument |
| `0x10002` | The required reader mode is not open |
| `0x10003` | The other reader mode (FeliCa or generic NFC) is already open |
| `0x20001` | Device error, unsupported command, or reader mode already open |
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

### Implementation details

* **Terminal authentication and payment use different reader paths.** Terminal
  authentication reads the setup card through the emulated Aime reader, as
  described in the e-money guide. Payment card detection uses the Thinca reader
  adapter in 64-bit amdaemon. The game process reads the same INI file. A 32-bit
  game process skips ICAS. A 64-bit game process installs the same hooks, which
  stay unused.
* **The client obtains a reader in two ways.** It calls `CreateThincaRwAdapter`,
  or amdaemon passes it a built-in reader directly through `setRwAdapter`. ICAS
  intercepts both. The factory is looked up by name or by export number
  (ordinal) 2. In ThincaPayment 1.4.11, `setRwAdapter` is entry 8 in the virtual
  method table, at offset `0x4e200` from the loaded DLL's base address. The hook
  checks the version and address before replacing that entry.
* **The factory hook is restored before payment initialization.** Later
  platform hooks overwrite the same `GetProcAddress` import. ICAS re-installs
  its hook immediately before the payment client initializes and chains to the
  hook it replaces, so both stay active.
* **The original reader stays with amdaemon.** Amdaemon can keep its built-in
  reader and register it again, so ICAS leaves that object intact. When
  registration succeeds, the payment client owns the emulated reader. When
  registration fails, ICAS frees the reader it allocated.
* **Payment scans share the existing AimeIO instance.** Polling and reading the
  card ID happen together under the normal reader lock. Until the backend has
  started, the bridge returns `E_PENDING` and ICAS retries within the scan
  timeout. AimeIO mode reads only from the reader, never from a card file.
* **The bytes after the IDm are a length, not part of the card ID.** In the
  27-byte detection payload, the eight IDm bytes begin at offset 9. The next two
  bytes, `00 08`, give the PMm length. The card ID is exactly 16 hexadecimal
  characters. The adapter returns IDm and PMm separately. ThincaPayment adds the
  result, type, and length fields (23 bytes), and the TCAP client adds a
  four-byte prefix. See [payment card detection](emoney.md#payment-card-detection)
  for the layout.
* **File and AimeIO access codes use the same hash input.** Both paths hash 20
  ASCII digits, including leading zeroes, with no whitespace or terminating null
  byte. The same code gives the same payment ID and keeps the company code as
  part of the input.

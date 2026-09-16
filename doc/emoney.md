# E-Money setup

Original authentication notes by Haruka Akechi.

Segatools emulates the authentication card used to register the terminal and the
ICAS reader used for payment cards. The game's ThincaPayment client handles
communication with your payment server.

## Setup

### 1. Generate the authentication data

Obtain the game's 64-byte authentication-card encryption key from `amdaemon.exe`.
From the `emoney` directory, install the dependencies and run the generator with
that key as 128 hexadecimal characters:

```shell
python -m pip install -r requirements.txt
python authcardgen.py --key YOUR_128_HEX_CHARACTER_KEY
```

The output is `authdata.bin`. Copy it into the game's `DEVICE` directory.

Use these options to match the registration details configured on your server:

| Option | Purpose |
| --- | --- |
| `--store-card-id` | Store card identifier, up to 16 bytes |
| `--merchant-code` | Merchant code, up to 20 bytes |
| `--store-branch-id` | Non-zero branch number, up to 12 bytes |
| `--passphrase` | Password for the PFX certificate returned by the server, up to 16 bytes |
| `--cardid` | Override the static authentication card ID with 64 hexadecimal characters |
| `--output` | Output path; defaults to `authdata.bin` |

Run `python authcardgen.py --help` for the full command reference.

### 2. Configure segatools

Merge these settings into `segatools.ini`:

```ini
[epay]
enable=1
hook=0

[aime]
enable=1
authdataPath=DEVICE\authdata.bin
```

`hook=0` selects the real ThincaPayment client. `authdataPath` is relative to the
daemon's working directory and defaults to `DEVICE\authdata.bin`.

If the game's `tfps-res-pro\env.json` has `"use_proxy": true`, also set
`proxyFlag=3` in the existing `[aime]` section. Its default is `2`.

### 3. Configure the payment server connection

Use the endpoint URLs and certificates supplied by your payment server:

| File | Setting |
| --- | --- |
| `tfps-res-pro\env.json` | Set `tasms.root_endpoint` to the terminal registration endpoint |
| `tfps-res-pro\resource.xml` | Set the service URLs, including `commonPrimaryUri`, to your server's endpoints |
| `ca.pem`, beside `amdaemon.exe` | Include the CA certificates needed by your server's TLS setup |

The endpoint hostnames must match the server's TLS certificate. The PFX returned
by terminal registration must use the passphrase from your authentication data.

### 4. Register the terminal

Open **E-Money Settings → Terminal Authentication** in the game's test menu.
Hold the configured Aime scan key (Enter by default) when prompted. The shop name
appears when registration completes.

Then follow [ICAS reader setup](icas.md) to configure payment card input from a
file or an AimeIO reader. ICAS is tested in SDHD.

## Troubleshooting

Set `emoney.log.level` to `4` in the game's amdaemon `config.json`. Check
`<amfs>\emoney_log\thincapayment.log`, the VFD message, and the segatools output.

| Symptom | Check |
| --- | --- |
| Authentication data cannot be read | `[aime] authdataPath` and the daemon's working directory |
| Unsupported card reader | Terminal authentication requires reader generation 3 or later; check `[aime] gen` |
| `setClientCertificate(). ErrCode 203` | Downloaded PFX file and its password |
| TCAP error `06514086` | Server CA entries in `ca.pem` |
| Registration works but payment taps fail | `[epay] hook=0` and the [ICAS settings](icas.md#configuration) |

## Technical info

### Authentication card

The emulated MIFARE reader serves authentication data from
[sg-nfc.c](../common/board/sg-nfc.c). Block 4 starts with `54 43`, followed by
`[aime] proxyFlag` and `01`. Encrypted data is served from `authdataPath` through
blocks 5, 6, 8, 9, 10, 12, 13, and 14.

[authcardgen.py](../emoney/authcardgen.py) constructs these fields before encryption:

| Field | Size |
| --- | --- |
| Store card ID | 16 bytes |
| Merchant code | 20 bytes |
| Store branch ID | 12 bytes |
| PFX passphrase | 16 bytes |
| Null terminator | 1 byte |

The generator pads the text fields with zeroes. It XORs each byte of the 64-byte
key with `0x1C`, then computes HMAC-SHA-256 over the static 32-byte card ID using
that modified key. The digest is the AES key; XORing its two 16-byte halves gives
the IV. The payload is encrypted with AES-CBC and PKCS#7 padding.

### Registration and service discovery

ThincaPayment sends the terminal details to `tasms.root_endpoint`:

```json
{
  "modelName": "ACA",
  "serialNumber": "ACAE01A9999",
  "merchantCode": "NOTSEGA",
  "storeBranchNumber": 11111,
  "storeCardId": "FAKESTORE"
}
```

`serialNumber` comes from the PCBID, while the merchant, branch, and store-card
values come from the authentication data. The response supplies a base64-encoded
PFX certificate and `initSettings` containing the terminal ID, service endpoints,
and available payment brands. The `x-certificate-md5` header contains the MD5 of
the decoded PFX bytes, not the base64 text.

The client then requests `initauth.jsp` relative to `commonPrimaryUri` for TLAM
discovery. Keep the trailing slash on the configured base URI.
The response uses `Content-Type: application/x-tlam` and names the TCAP endpoint:

```text
SERV=https://payment.example/emoney/tcap
```

The discovery and payment exchanges use these entry points relative to the
configured service URLs:

| Entry point | Purpose |
| --- | --- |
| `initauth.jsp` | Terminal initialization; `DirectIO` returns authorization status and floor limit |
| `emlist.jsp` | Brand discovery; `DirectIO` returns parallel `EMoneyCode` and `URL` arrays |
| `tlamAuthorizeSales.jsp` / `payment.jsp` | Payment at the selected brand's service URL |
| `balanceInquiry.jsp` | Balance inquiry at the selected brand's service URL |

Each TLAM response supplies its own `SERV=` destination for binary TCAP POSTs.
An empty POST body begins the application exchange; it is not a five-byte TCAP
frame with packet type `FF`. Initialization requests carry XML properties,
including JSON in `AdditionalSecurityInformation`. Initialization uses
`UniqueCode` and brand discovery uses `TermSerial` to identify the terminal.

### TCAP frames

ThincaPayment exchanges binary frames with that endpoint using
`Content-Type: application/x-tcap`. Segatools implements the reader calls made
by the client; the server controls the payment flow.

| Frame offset | Size | Field |
| --- | --- | --- |
| 0 | 2 | Protocol marker, `02 05` |
| 2 | 1 | Packet type |
| 3 | 2 | Big-endian body length, excluding the five-byte header |

Packet types are `01` handshake, `02` farewell, `03` error, `04` application data,
`05` entity update, and `06` entity operation. The body contains subpackets with
a six-byte header followed by their payload:

```text
opcode high | parameter 1 | parameter 2 | opcode low | payload length (2 bytes)
```

The payload length is big-endian and excludes the subpacket header. Opcode
meanings depend on the packet type. HTTP chunk boundaries are separate from
TCAP frame boundaries.

For example, this handshake response contains 21 body bytes:

```text
02 05 01 00 15
00 00 00 23 00 00
00 00 00 81 00 03 02 05 00
00 00 00 24 00 00
```

It contains an empty `0x23` subpacket, acceptance `0x81` with payload
`02 05 00`, and an empty `0x24` completion subpacket.

A farewell response uses packet type `02`, with `0x23`, `0x25` carrying a
four-byte return value, and `0x24`. The same opcode can have different meanings
in another packet type; `0x25` in an entity-operation packet is a device command.

### Entity operations

Packet type `06`, subpacket opcode `0x25`, carries this command envelope:

| Field | Size |
| --- | --- |
| Command length | 1 byte |
| Command | That many bytes; ASCII name or a binary command byte |
| Reserved | 2 bytes, `00 00` |
| Argument length | 2 bytes, big-endian |
| Arguments | That many bytes |

Commands without arguments end with a zero argument length. Reader and LED
commands carry raw argument bytes. XML and status arguments include an
additional two-byte big-endian content length, counted in the outer argument
length. These lengths are separate from the frame and subpacket lengths.

| Command | Subpacket parameters | Purpose / arguments |
| --- | --- | --- |
| `REQUEST` | `00 01` | Ask for the client's current request data |
| `CURRENT` | `00 01` | Supply response XML and receipt properties |
| `RESULT` | `00 01` | Complete with response XML and propagate its result code |
| `STATUS` | `00 03` | Eight zero bytes followed by device-event JSON, inside the content-length wrapper |
| Binary `31` | `00 05` | Reader LED operation with raw arguments |
| `OPEN_RW` | `00 08` | Raw `00 00 09`: reader 0 and big-endian card-type mask `0009` |
| `TARGET_DETECT` | `00 08` | Raw four-byte big-endian timeout in milliseconds |

A card prompt sends `STATUS`, LED control, `OPEN_RW`, `TARGET_DETECT`, then
`REQUEST` in one entity-operation frame. For a five-second detection timeout,
the raw `TARGET_DETECT` argument is `00 00 13 88`.

The LED argument used for this prompt is
`00 00 03 03 02 00 08 00 00 13 88 01 F4 01 F4`.
The status JSON contains `deviceType`, `deviceNumber`, `actionType`, `sendData`,
and `screen`. Its six-byte `sendData` holds the brand, message ID, and display
timeout as three big-endian 16-bit values. The display timeout is independent
of the reader's detection timeout.

### Payment card detection

ThincaPayment translates TCAP reader requests into ICAS calls. Segatools returns
the card identity selected by the `[icas]` settings, which the client sends to
the server for a balance inquiry or payment.

A successful generic NFC detection payload is 27 bytes:

| Payload offset | Size | Field |
| --- | --- | --- |
| 0 | 9 | Detection metadata |
| 9 | 8 | FeliCa IDm |
| 17 | 2 | PMm length, `00 08` |
| 19 | 8 | PMm |

The card ID is exactly eight bytes at offset 9. The following `0008` is the PMm
length and must not be included in the ID. See the
[ICAS technical notes](icas.md#technical-info) for adapter and AimeIO details.

### Transaction responses and completion

The client's device-response payload contains a six-byte prefix followed by
request XML. A payment request uses service `AuthorizeSales` and supplies
`Amount`, `PaymentMedia`, and `SequenceNumber`. The sequence number is echoed in
the response. PASELI uses payment-media value `8`.

Response XML carries typed properties inside `userdata/properties`:

| Property | Purpose |
| --- | --- |
| `ResultCode`, `ResultCodeExtended` | Transaction outcome; zero indicates success |
| `SequenceNumber` | Matches the request |
| `Balance`, `SettledAmount` | Remaining balance and amount settled |
| `AccountNumber` | Server-side account identifier |
| `AdditionalSecurityInformation` | XML-escaped JSON containing receipt or initialization data |
| `CenterResultCode` | Server result detail |

A successful payment uses `CURRENT` with the response XML. A balance inquiry
also returns the balance, with `SettledAmount=0` and receipt type `5` rather
than payment receipt type `0`. A declined payment sends a status message followed
by `RESULT` with a non-zero `ResultCode` and `SettledAmount=0`. `CURRENT` updates
receipt data alone; using it for a decline can leave the client's completion
result at zero.

After an initialization or brand-discovery `CURRENT`, the client can acknowledge
with the six-byte payload `00 00 00 02 00 00`. This is a completion acknowledgement,
not another XML request; the server finishes with a farewell instead of sending
the initialization response again.

### Server reference

[miniThincaServer](https://github.com/TeddyKagurazaka/miniThincaServer) provides a
server-side reference for these exchanges:

* [Models.cs](https://github.com/TeddyKagurazaka/miniThincaServer/blob/main/miniThincaLib/Models.cs): packet types, framing, and parsing.
* [Builder.cs](https://github.com/TeddyKagurazaka/miniThincaServer/blob/main/miniThincaLib/Builder.cs): response generation, including the handshake above.
* [TcapHandler.cs](https://github.com/TeddyKagurazaka/miniThincaServer/blob/main/miniThincaLib/TcapHandler.cs): request dispatch and transaction flow.

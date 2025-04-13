# E-Money Authentication Procedure
by Haruka Akechi

### SETTING UP

1) Obtain the 64 byte long authentication card encryption key and the 32 byte long static authentication card ID. amdaemon.exe holds the secrets.

2) Get this java file, insert the ID and key, probably edit the passphrase and compile+run to generate authcard.bin: https://gist.github.com/akechi-haruka/a506184638e695a04eabe8cb53f62c36

3) Place authcard.bin in your DEVICE folder.

4) Check tfps-res-pro\env.json for your game. If it contains a "use_proxy: true" statement, add "proxy_flag=3" under [aime]

5) Replace the two URLs in tfps-res-pro\resource.xml to your servers'. This is to ensure the Host header will match the certificate's.

6) Where amdaemon.exe is located, there should be a "ca.pem". Replace this file with either [this](https://curl.se/ca/cacert.pem) for the most common CA's (including Let's Encrypt), or whatever CA the server is using (your server will provide this).

7) Run your game and enter the test menu, and navigate to E-Money Settings.

8) Select "Terminal Authentication"

9) Hold your key for scanning a card (default: RETURN)

10) If your shop name shows up, everything was done succesfully. Otherwise, check the VFD.

### TECHNICAL INFO

For debugging anything e-money related, I highly recommend setting "emoney.log.level" to 4 in your game's amdaemon config.json. This should create a `<amfs>\emoney_log\thincapayment.log`.

When terminal authentication is started from the test menu, the game will check for an Aime reader of at least generation 3. If that is not fulfilled, the VFD will display "unsupported card reader" and abort. If the card reader is good, the VFD will prompt for a card to be touched, and the reader will start scanning for a MIFARE card, which from this point we call "Thinca Authentication Card". This card contains one unencrypted block (3) which contains:

[0] = 0x54 // T

[1] = 0x43 // C

[2] = proxy_type

[3] = 0x01

Afterwards, a number of encrypted blocks are read, namely the blocks 5,6,8,9,10,12,13 and 14. These blocks together form a 130 byte long binary blob that contains the authentication data.

This data is encrypted as following:

Given a fixed 0x40 byte long encryption key and a fixed 0x20 byte long static "card ID", both of which can be found in amdaemon:
XOR every byte of the encryption key with 0x1C.
Calculate a 0x20 bytes long HMAC-SHA-256 of the card ID with the XOR'ed encryption key as the key.
Calculate the needed IV by XORing the lower 0x10 bytes of the HMAC with the upper 0x10 bytes of the HMAC:

```
byte[] iv = new byte[16];
for (int i = 0; i < 16; i++) {
    iv[i] = (byte) (hmac[i + 16] ^ hmac[i]);
}
```

With this IV, and the HMAC as the key, finally encrypt the data with AES/CBC/PKCS5Padding.

Now what is actually stored on such a card? This:

```
+---------------+---------------+-----------------+------------+----------+
| Store Card ID | Merchant Code | Store Branch ID | Passphrase | NULL     |
+---------------+---------------+-----------------+------------+----------+
| 0x10 bytes    | 0x14 bytes    | 0xC bytes       | 0x10 bytes | 0x1 byte |
| char*         | char*         | uint128_t       | char*      | NULL     |
+---------------+---------------+-----------------+------------+----------+
```

Only two things really matter here. The Store Branch ID must be non-zero, otherwise amdaemon will reject it, and the passphrase, which is the PFX key password for the certificate returned in the network response (see below).

Technically with the Store Card ID you could bind different auth cards to different users, but for home usage, it really doesn't matter.

That's the Thinca Authentication Card out of the way, so continue on to:

### NETWORK

First, a regular HTTP(S) connection will be made to the URL specified in tfps-res-pro\env.json, tasms.root_endpoint.

Request Data:
`{"modelName":"ACA","serialNumber":"ACAE01A9999","merchantCode":"NOTSEGA","storeBranchNumber":11111,"storeCardId":"FAKESTORE"}`

Note that the serialNumber here actually isn't the keychip ID, but rather the PCBID. The three other values are read from the Thinca Authentication Card.

Response Headers:
`x-certificate-md5: <md5 of the certificate string>`

Response Data:
```
{
  "certificate": "<base 64 encoded pfx string, with the authentication card's passphrase being the pfx password>",
  "initSettings": {
    "endpoints": {
      "terminals": {
        "uri": "https://localhost/emoney/terminals"
      },
      "statuses": {
        "uri": "https://localhost/emoney/statuses"
      },
      "sales": {
        "uri": "https://localhost/emoney/sales"
      },
      "counters": {
        "uri": "https://localhost/emoney/counters"
      }
    },
    "intervals": {
      "checkSetting": 60,
      "sendStatus": 60
    },
    "settigsType": "AmusementTerminalSettings", // sic
    "status": "1", // a string
    "terminalId": "536453645364536453645364536453", // must be exactly 30 characters
    "version": "2024-01-01T01:01:01", // a timestamp
    "availableElectronicMoney": [
      1,
      2,
      3,
      5,
      6,
      8,
      9,
      91, // aimepay
      101 // "cash" ???
    ],
    "cashAvailability": true,
    "productCode": 1337
  }
}
```

Next up, we will connect to the TLAM service to get the URL for the TCAP service. Everything from this point on requires not only HTTPS, but also client certificate validation (which is the certificate returned from the previous request). Technically you don't need to validate it, but you must accept a client certificate or the client HTTPS library will not be happy.

The client certificate itself must be signed with the same key than the server's HTTPS certificate.

The client AND server certificate must have it's CA included in the "ca.pem" file in amdaemon's directory. You can freely replace this file with https://curl.se/ca/cacert.pem to allow Let's Encrypt and whatever else.

At this point, "ThincaPayment::setClientCertificate(). ErrCode 203" means that the downloaded file couldn't be found, or the certificate password is wrong.

[Warn ] TCAP communicate error 06514086 means the ca.pem has no entry for the given server CA.

TLAM:
The TLAM url comes from tftp-res-pro\resource.xml, commonPrimaryUri.

<commonPrimaryUri>/initauth.jsp

Request Data:
<none>
Response Headers:
`Content-Type: application/x-tlam`
Response Data:
`SERV=https://localhost/emoney/tcap`

TCAP:
Here be dragons.

Is it this?? https://en.wikipedia.org/wiki/Transaction_Capabilities_Application_Part
Maybe thincatcapclient.dll holds the secrets?

Request Data:
```
02 05 01 00 ba 00 00 00  21 00 00 00 00 00 25 00    ....º... !.....%.
9f 00 01 00 00 07 47 65  6e 65 72 69 63 06 43 4c    ......Ge neric.CL
49 45 4e 54 00 02 00 00  07 47 65 6e 65 72 69 63    IENT.... .Generic
06 53 54 41 54 55 53 00  03 00 00 07 47 65 6e 65    .STATUS. ....Gene
72 69 63 06 4f 50 54 49  4f 4e 00 04 00 00 06 46    ric.OPTI ON.....F
65 6c 69 43 61 03 52 2f  57 00 05 00 00 07 47 65    eliCa.R/ W.....Ge
6e 65 72 69 63 09 52 2f  57 5f 45 56 45 4e 54 00    neric.R/ W_EVENT.
06 00 00 07 47 65 6e 65  72 69 63 0a 52 2f 57 5f    ....Gene ric.R/W_
53 54 41 54 55 53 00 07  00 00 07 47 65 6e 65 72    STATUS.. ...Gener
69 63 0a 52 2f 57 5f 4f  50 54 49 4f 4e 00 08 00    ic.R/W_O PTION...
00 07 47 65 6e 65 72 69  63 06 4e 46 43 5f 52 57    ..Generi c.NFC_RW
00 00 00 26 00 03 02 05  00 00 00 00 22 00 00       ...&.... ...."..
```

and
```
02 05 03 00 17 00 00 00  21 00 11 34 34 20 30 30    ........ !..44 00
20 30 31 20 32 31 20 30  30 20 30 30                 01 21 0 0 00
```

Response Headers:
```
Content-Type: application/x-tcap
Transfer-Encoding: chunked
```
Response Data:
????

To be continued ...
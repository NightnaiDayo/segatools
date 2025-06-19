import hmac
import hashlib
import click

from Crypto.Cipher import AES
from Crypto.Util.Padding import pad, unpad
from pathlib import Path


class CardEncryptor:
    def __init__(
        self,
        cardid_hex,
        key_hex,
        store_card_id_str="FAKESTORE",
        merchant_code_str="NOTSEGA",
        store_branch_id_str="11111",
        passphrase_str="573",
        output_file="authdata.bin",
    ):
        self.cardid = bytes.fromhex(cardid_hex)
        self.key = bytearray.fromhex(key_hex)
        self.output_file = output_file

        if len(self.cardid) != 0x20:
            raise ValueError("Card ID must be 32 bytes (64 hex characters)")
        if len(self.key) != 0x40:
            raise ValueError("Key must be 64 bytes (128 hex characters)")

        # XOR the key with 0x1C as in original Java
        for i in range(len(self.key)):
            self.key[i] ^= 0x1C

        self.store_card_id = self._str_to_bytes(store_card_id_str, 0x10)
        self.merchant_code = self._str_to_bytes(merchant_code_str, 0x14)
        self.store_branch_id = self._str_to_bytes(store_branch_id_str, 0x0C)
        self.passphrase = self._str_to_bytes(passphrase_str, 0x10)

        # Construct full data payload
        self.data = (
            self.store_card_id
            + self.merchant_code
            + self.store_branch_id
            + self.passphrase
            + bytes([0x00])  # +1 null terminator / padding byte
        )

    def _str_to_bytes(self, s, length):
        b = bytearray(length)
        b[: len(s)] = s.encode()
        return bytes(b)
    
    def _bytes_to_str(self, b):
        return b.decode()

    def _bytes_to_hex(self, b):
        return b.hex().upper()

    def _calculate_hmac_sha256(self, key, data, length):
        h = hmac.new(key, data, hashlib.sha256)
        return h.digest()[:length]

    def _aes_cbc_encrypt(self, key, data, iv):
        cipher = AES.new(key, AES.MODE_CBC, iv)
        return cipher.encrypt(pad(data, AES.block_size))

    def _aes_cbc_decrypt(self, key, data, iv):
        cipher = AES.new(key, AES.MODE_CBC, iv)
        return unpad(cipher.decrypt(data), AES.block_size)

    def run(self):
        print("Card ID:\t\t", self._bytes_to_hex(self.cardid))
        print("Store Card ID:\t\t", self._bytes_to_str(self.store_card_id))
        print("Merchant Code:\t\t", self._bytes_to_str(self.merchant_code))
        print("Store Branch ID:\t", self._bytes_to_str(self.store_branch_id))
        print("Passphrase:\t\t", self._bytes_to_str(self.passphrase))

        hmac_output = self._calculate_hmac_sha256(self.key, self.cardid, 0x20)
        # print("HMAC:\t\t", self._bytes_to_hex(hmac_output))

        iv = bytes([hmac_output[i + 16] ^ hmac_output[i] for i in range(16)])
        # print("IV:\t\t", self._bytes_to_hex(iv))

        encrypted = self._aes_cbc_encrypt(hmac_output, self.data, iv)
        # print("ENCRYPTED:\t", self._bytes_to_hex(encrypted))
        Path(self.output_file).write_bytes(encrypted)

        decrypted = self._aes_cbc_decrypt(hmac_output, encrypted, iv)
        # print("DECRYPTED:\t", self._bytes_to_hex(decrypted))


@click.command()
@click.option(
    "--cardid",
    default="0102030401020304010203040102030401020304010203040102030401020304",
    help="Card ID (64 hex characters)",
)
@click.option(
    "--key",
    required=True,
    help="Key (128 hex characters, required)",
)
@click.option(
    "--store-card-id", default="FAKESTORE", help="Store Card ID (padded to 16 bytes)"
)
@click.option(
    "--merchant-code", default="NOTSEGA", help="Merchant Code (padded to 20 bytes)"
)
@click.option(
    "--store-branch-id", default="11111", help="Store Branch ID (padded to 12 bytes)"
)
@click.option("--passphrase", default="573", help="Passphrase, used for the pfx password (padded to 16 bytes)")
@click.option("--output", default="authdata.bin", help="Output filename")
def cli(cardid, key, store_card_id, merchant_code, store_branch_id, passphrase, output):
    if len(key) != 128 or not all(c in "0123456789abcdefABCDEF" for c in key):
        raise click.BadParameter("The key must be a 128-character hexadecimal string.")

    encryptor = CardEncryptor(
        cardid,
        key,
        store_card_id_str=store_card_id,
        merchant_code_str=merchant_code,
        store_branch_id_str=store_branch_id,
        passphrase_str=passphrase,
        output_file=output,
    )
    encryptor.run()


if __name__ == "__main__":
    cli()

#!/usr/bin/env python3
"""Recover the symmetric key of a public IPSW .aea image.

Reads the AEA1 auth data (first bytes of the file), downloads the private key
Apple publishes at com.apple.wkms.fcs-key-url (wkms-public.apple.com), and opens
com.apple.wkms.fcs-response with HPKE base mode, suite DHKEM(P-256, HKDF-SHA256)
/ HKDF-SHA256 / AES-256-GCM (RFC 9180). The AES-GCM tag authenticates the result.

  aea-key.py HEADER.bin   -> prints base64:<key> for `aea decrypt -key-value`
"""
import base64, hashlib, hmac, json, struct, sys, urllib.parse, urllib.request
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.ciphers.aead import AESGCM


def auth_data(blob):
    if blob[:4] != b'AEA1':
        sys.exit('not an AEA1 file')
    size, = struct.unpack_from('<I', blob, 8)
    data, fields, pos = blob[12:12 + size], {}, 0
    while pos < len(data):
        length, = struct.unpack_from('<I', data, pos)
        if length < 4 or pos + length > len(data):
            sys.exit('malformed auth data')
        key, _, value = data[pos + 4:pos + length].partition(b'\0')
        fields[key.decode()] = value
        pos += length
    return fields


def extract(salt, ikm):
    return hmac.new(salt or b'\0' * 32, ikm, hashlib.sha256).digest()


def expand(prk, info, length):
    out, block, counter = b'', b'', 1
    while len(out) < length:
        block = hmac.new(prk, block + info + bytes([counter]), hashlib.sha256).digest()
        out += block
        counter += 1
    return out[:length]


def hpke_open(private_key, enc, ciphertext, info=b'', aad=b''):
    kem_suite = b'KEM' + (0x0010).to_bytes(2, 'big')
    suite = b'HPKE' + (0x0010).to_bytes(2, 'big') + (0x0001).to_bytes(2, 'big') + (0x0002).to_bytes(2, 'big')

    def labeled_extract(sid, salt, label, ikm):
        return extract(salt, b'HPKE-v1' + sid + label + ikm)

    def labeled_expand(sid, prk, label, ctx, length):
        return expand(prk, length.to_bytes(2, 'big') + b'HPKE-v1' + sid + label + ctx, length)

    pk_e = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), enc)
    dh = private_key.exchange(ec.ECDH(), pk_e)
    pk_r = private_key.public_key().public_bytes(serialization.Encoding.X962,
                                                 serialization.PublicFormat.UncompressedPoint)
    eae_prk = labeled_extract(kem_suite, b'', b'eae_prk', dh)
    shared = labeled_expand(kem_suite, eae_prk, b'shared_secret', enc + pk_r, 32)
    context = (b'\0' + labeled_extract(suite, b'', b'psk_id_hash', b'')
               + labeled_extract(suite, b'', b'info_hash', info))
    secret = labeled_extract(suite, shared, b'secret', b'')
    key = labeled_expand(suite, secret, b'key', context, 32)
    nonce = labeled_expand(suite, secret, b'base_nonce', context, 12)
    return AESGCM(key).decrypt(nonce, ciphertext, aad)


def main():
    fields = auth_data(open(sys.argv[1], 'rb').read())
    url = fields['com.apple.wkms.fcs-key-url'].decode()
    host = urllib.parse.urlparse(url).hostname
    if host != 'wkms-public.apple.com':
        sys.exit(f'refusing key URL outside wkms-public.apple.com: {host}')
    pem = urllib.request.urlopen(url, timeout=60).read()
    private_key = serialization.load_pem_private_key(pem, password=None)
    response = json.loads(fields['com.apple.wkms.fcs-response'])
    key = hpke_open(private_key, base64.b64decode(response['enc-request']),
                    base64.b64decode(response['wrapped-key']))
    print('base64:' + base64.b64encode(key).decode())


if __name__ == '__main__':
    main()

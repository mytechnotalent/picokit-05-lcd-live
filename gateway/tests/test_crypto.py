"""Known-answer and tamper tests for the gateway crypto module.

The vectors match the RP2350 firmware and the reference cold chain
monitor test suite, so a passing run proves byte-for-byte
interoperability with the embedded implementation.

Parameters
----------
None

Returns
-------
None
"""

import crypto
import pytest

RFC_TAG = "0d640df58d78766c08c037a34a8b53c9d01ef0452d75b65eb52520e96b01e659"
FIELD_KEY = "b6274f387d70967c7c3dc10a742e1c6a6f42002c0d8c9f29d841ad98047a1af7"
FRAME_CT = "e0f2ae2cf99363be99295ff2b5cbb509ff6754ad"
FRAME_TAG = "2d0f4a77a547af59e27df8c05738c733"
PLAINTEXT = b'{"n":1,"s":12,"b":2}'


def test_rfc9106_argon2id_vector():
    """Verify the RFC 9106 Argon2id known-answer vector.

    Parameters
    ----------
    None

    Returns
    -------
    None
    """
    tag = crypto._argon2id(bytes([1]) * 32, bytes([2]) * 16, 3, 32, 4,
                           32, 2, bytes([3]) * 8, bytes([4]) * 12)
    assert tag.hex() == RFC_TAG


def test_field_key_matches_firmware():
    """Verify the field profile key matches the firmware vector.

    Parameters
    ----------
    None

    Returns
    -------
    None
    """
    assert crypto.derive_field_key().hex() == FIELD_KEY


def test_envelope_matches_firmware():
    """Verify a sealed frame matches the firmware byte layout.

    Parameters
    ----------
    None

    Returns
    -------
    None
    """
    nonce = bytes(range(24))
    ciphertext, tag = crypto._xchacha_seal(
        crypto.derive_field_key(), nonce, bytes([5]), PLAINTEXT)
    assert ciphertext.hex() == FRAME_CT
    assert tag.hex() == FRAME_TAG


def test_public_aead_round_trip():
    """Verify the public AEAD wrappers seal and open a frame.

    Parameters
    ----------
    None

    Returns
    -------
    None
    """
    key = crypto.derive_field_key()
    nonce = bytes(range(24))
    ciphertext, tag = crypto.xchacha_seal(key, nonce, b"\x05", PLAINTEXT)
    opened = crypto.xchacha_open(key, nonce, b"\x05", ciphertext, tag)
    assert opened == PLAINTEXT


def test_seal_open_round_trip():
    """Verify a sealed hex envelope opens back to the plaintext.

    Parameters
    ----------
    None

    Returns
    -------
    None
    """
    envelope = crypto.seal_field_frame(PLAINTEXT)
    assert crypto.open_field_frame(envelope) == PLAINTEXT


def test_flipped_tag_rejected():
    """Verify a flipped authentication tag is rejected.

    Parameters
    ----------
    None

    Returns
    -------
    None
    """
    raw = bytearray(bytes.fromhex(crypto.seal_field_frame(PLAINTEXT)))
    raw[-1] ^= 0x01
    with pytest.raises(ValueError):
        crypto.open_field_frame(bytes(raw).hex())


def test_flipped_ciphertext_rejected():
    """Verify flipped ciphertext is rejected.

    Parameters
    ----------
    None

    Returns
    -------
    None
    """
    raw = bytearray(bytes.fromhex(crypto.seal_field_frame(PLAINTEXT)))
    raw[24] ^= 0x01
    with pytest.raises(ValueError):
        crypto.open_field_frame(bytes(raw).hex())


def test_malformed_frame_rejected():
    """Verify malformed hexadecimal frames are rejected.

    Parameters
    ----------
    None

    Returns
    -------
    None
    """
    for candidate in ("", "00", "zz"):
        with pytest.raises(ValueError):
            crypto.open_field_frame(candidate)

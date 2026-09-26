#!/usr/bin/env python3

import base64
import hashlib
import hmac
import struct
import unittest


DOMAIN = b"LORA-APRS-RC"


def authenticated_bytes(controller: str, target: str, counter: int, command: str) -> bytes:
    controller_bytes = controller.encode("ascii")
    target_bytes = target.encode("ascii")
    command_bytes = command.encode("ascii")
    return b"".join(
        (
            DOMAIN,
            b"\x01",
            struct.pack(">H", len(controller_bytes)),
            controller_bytes,
            struct.pack(">H", len(target_bytes)),
            target_bytes,
            b"A",
            struct.pack(">Q", counter),
            struct.pack(">H", len(command_bytes)),
            command_bytes,
        )
    )


def tag(key: bytes, controller: str, target: str, counter: int, command: str) -> str:
    digest = hmac.new(
        key, authenticated_bytes(controller, target, counter, command), hashlib.sha256
    ).digest()[:12]
    return base64.urlsafe_b64encode(digest).rstrip(b"=").decode("ascii")


class RemoteAuthVectors(unittest.TestCase):
    def test_published_vector(self):
        key = bytes(range(32))
        self.assertEqual(
            base64.urlsafe_b64encode(key).rstrip(b"=").decode("ascii"),
            "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8",
        )
        self.assertEqual(
            authenticated_bytes("F4MLV-2", "F4MLV-10", 71, "TX=OFF").hex(),
            "4c4f52412d415052532d524301000746344d4c562d32000846344d4c562d3130"
            "410000000000000047000654583d4f4646",
        )
        self.assertEqual(
            tag(key, "F4MLV-2", "F4MLV-10", 71, "TX=OFF"),
            "OLkaJxyKIXBM9SrF",
        )

    def test_every_bound_field_changes_the_tag(self):
        key = bytes(range(32))
        baseline = tag(key, "F4MLV-2", "F4MLV-10", 71, "TX=OFF")
        variants = (
            tag(key, "F4MLV-3", "F4MLV-10", 71, "TX=OFF"),
            tag(key, "F4MLV-2", "F4MLV-11", 71, "TX=OFF"),
            tag(key, "F4MLV-2", "F4MLV-10", 72, "TX=OFF"),
            tag(key, "F4MLV-2", "F4MLV-10", 71, "TX=ON"),
        )
        self.assertTrue(all(value != baseline for value in variants))


if __name__ == "__main__":
    unittest.main()

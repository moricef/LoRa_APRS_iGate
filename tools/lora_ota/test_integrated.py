import unittest
from send_integrated import *

class WireTests(unittest.TestCase):
    key=bytes(range(32))
    source=b'F4MLV-2'
    target=b'F4MLV-15'
    nonce=bytes(range(8))

    def reply(self,payload):
        return make_frame(self.key,self.target,self.source,self.nonce,ACK,204,payload)
    def decode(self,raw):
        return decode_reply(self.key,self.source,self.target,raw)
    def test_maximum_data_frame(self):
        f=make_frame(self.key,self.source,self.target,self.nonce,DATA,0,bytes(CHUNK))
        self.assertEqual(len(f),255)
        with self.assertRaises(ValueError):make_frame(self.key,self.source,self.target,self.nonce,DATA,0,bytes(CHUNK+1))
    def test_short_reply_and_authentication(self):
        f=self.reply(bytes([DATA,0])+struct.pack('>I',408))
        self.assertEqual(self.decode(f)['written'],408)
        self.assertIsNone(self.decode(f[:-1]))
        self.assertIsNone(self.decode(f[:-1]+bytes([f[-1]^1])))
        self.assertIsNone(decode_reply(bytes(32),self.source,self.target,f))
        self.assertIsNone(decode_reply(self.key,b'F4MLV-3',self.target,f))
        self.assertIsNone(decode_reply(self.key,self.source,b'F4MLV-14',f))
    def test_metadata_and_malformed_authenticated_replies(self):
        short=bytes([STATUS,0])+struct.pack('>I',408)
        version=b'4.0.2RXT-OTA-B'
        payload=short+b'\1'+struct.pack('>II',0x1f0000,0x1f0000)+bytes([len(version)])+version+bytes(range(32))
        r=self.decode(self.reply(payload))
        self.assertTrue(r['pending']);self.assertEqual(r['version'],version.decode())
        self.assertEqual(r['image_sha'],bytes(range(32)).hex())
        for truncated in (payload[:7],payload[:15],payload[:-1]):
            self.assertIsNone(self.decode(self.reply(truncated)))
        bad=bytearray(payload);bad[6]=2
        self.assertIsNone(self.decode(self.reply(bad)))
        bad=bytearray(payload);bad[15]=41
        self.assertIsNone(self.decode(self.reply(bad)))
        bad=bytearray(payload);bad[16]=255
        self.assertIsNone(self.decode(self.reply(bad)))

if __name__=='__main__':unittest.main()

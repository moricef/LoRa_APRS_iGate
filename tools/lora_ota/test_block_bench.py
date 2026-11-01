import struct,unittest
from unittest.mock import Mock
from pathlib import Path
from block_bench import geometry,plan,counter_delta,summarize,preflight
from radio_profiles import load_profiles,RadioProfile
from window_transfer import transfer,START,DATA,QUERY
from send_integrated import make_frame,decode_reply,ACK

class BlockBenchTests(unittest.TestCase):
    def profiles(self):return load_profiles(Path(__file__).with_name('radio-profiles.example.json'))
    def test_geometry_and_wire_limits(self):
        self.assertEqual(geometry(32,False),(128,16))
        self.assertEqual(geometry(48,False),(86,12))
        self.assertEqual(geometry(64,False),(64,8))
        self.assertEqual(geometry(128,False),(32,4))
        self.assertEqual(geometry(175,True),(24,4))
        self.assertEqual(geometry(204,False),(21,4))
        for n,route in [(31,False),(205,False),(176,True),(True,False)]:
            with self.assertRaises(ValueError):geometry(n,route)
    def test_plan_reproducible_matched_repetitions(self):
        profiles=self.profiles();a=plan([32,64,128,175],5,42,profiles,True)
        self.assertEqual(a,plan([32,64,128,175],5,42,profiles,True))
        self.assertEqual(len(a),20)
        for i in range(0,20,4):self.assertEqual({r['block_bytes'] for r in a[i:i+4]},{32,64,128,175})
        self.assertTrue(all(r['data_frame_max_bytes']<=255 for r in a))
        self.assertEqual(a,plan([32,64,128,175],5,42,profiles,True))
    def test_slow_relay_burst_rejected_and_same_profile_direct_allowed(self):
        profiles=self.profiles();profiles['transfer']=profiles['normal']
        with self.assertRaisesRegex(ValueError,'lease'):plan([32],1,0,profiles,True)
        self.assertEqual(len(plan([32],1,0,profiles,False)),1)
    def test_single_frame_too_slow_for_lease_is_rejected_even_direct(self):
        profiles=self.profiles();profiles['transfer']=RadioProfile(434550000,7800,12,8,3)
        with self.assertRaisesRegex(ValueError,'too slow'):plan([128],1,0,profiles,False)
    def test_sample_uses_high_bits_and_real_page_tail_without_activation(self):
        for chunk in (32,48,64,96,128,160,175,192,204):
            count,width=geometry(chunk,False);tail=73
            c=Mock(retries=0);c.route=None
            c.request.side_effect=[{'written':0,'bitmap_bytes':width},
                {'written':4096,'bitmap':(1<<count)-1},
                {'written':4096+tail,'bitmap':(1<<((tail+chunk-1)//chunk))-1},
                {'written':4096+tail}]
            transfer(c,bytes(4096+tail),bytes(32),lambda *a,**k:None,block_size=chunk,verify_sample=True)
            calls=c.request.call_args_list
            self.assertEqual(calls[-1].args[0],26)
            self.assertEqual(len(calls[1].args[2]),width)
            self.assertEqual(len(calls[0].args[2]),40 if chunk==128 else 42)
            blocks=c.send_unacknowledged.call_args_list
            self.assertEqual(sum(len(call.args[2]) for call in blocks),4096+tail)
            self.assertTrue(all(len(call.args[2])<=4096-call.args[1]%4096 for call in blocks))
            self.assertEqual(blocks[count].args[1],4096)
    def test_upper_word_missing_block_is_repaired(self):
        c=Mock(retries=0);c.route=None;mask=(1<<128)-1
        c.request.side_effect=[{'written':0,'bitmap_bytes':16},{'written':0,'bitmap':mask^(1<<127)},
                               {'written':4096,'bitmap':mask},{'written':4096}]
        transfer(c,bytes(4096),bytes(32),lambda *a,**k:None,block_size=32,verify_sample=True)
        offsets=[call.args[1] for call in c.send_unacknowledged.call_args_list]
        self.assertEqual(offsets,list(range(0,4096,32))+[4064])
    def test_wrong_bitmap_width_cannot_silently_start_data(self):
        c=Mock(retries=0);c.route=None;c.request.return_value={'written':0,'bitmap_bytes':4}
        with self.assertRaisesRegex(RuntimeError,'negotiate'):transfer(c,bytes(4096),bytes(32),lambda *a,**k:None,block_size=32)
        c.send_unacknowledged.assert_not_called()
    def test_legacy_128_finish_is_unchanged(self):
        c=Mock(retries=0);c.route=None;c.request.side_effect=[{'written':0},{'written':4096,'bitmap':0xffffffff},{'written':4096}]
        transfer(c,bytes(4096),bytes(32),lambda *a,**k:None)
        self.assertEqual(c.request.call_args_list[-1].args[0],18)
    def test_extended_bitmap_and_stats_replies(self):
        key=bytes(range(32));source=b'F4MLV-2';target=b'F4MLV-15'
        for width in (4,8,12,16):
            payload=bytes([QUERY,0])+struct.pack('>I',0)+((1<<(width*8))-1).to_bytes(width,'big')
            wire=make_frame(key,target,source,bytes(8),ACK,0,payload)
            r=decode_reply(key,source,target,wire);self.assertEqual(r['bitmap_bytes'],width)
            self.assertEqual(r['bitmap'],(1<<(width*8))-1)
        payload=bytes([25,0])+struct.pack('>I',0)+struct.pack('>IIIIQ',9,11,0,1234,2**33)
        r=decode_reply(key,source,target,make_frame(key,target,source,bytes(8),ACK,0,payload))
        self.assertEqual(r['tx_airtime_us'],2**33);self.assertEqual(r['stats_boot'],9)
        p=payload+bytes(range(32))
        r=decode_reply(key,source,target,make_frame(key,target,source,bytes(8),ACK,0,p))
        self.assertEqual(r['elf_sha256'],bytes(range(32)).hex())
    def test_preflight_refusal_never_discards_existing_progress(self):
        c=Mock();c.hello.return_value={'pending':False};c.request.side_effect=RuntimeError('unsupported stats')
        with self.assertRaises(RuntimeError):preflight(c)
        self.assertEqual([call.args[0] for call in c.request.call_args_list],[25])
        c=Mock();c.hello.return_value={'pending':True}
        with self.assertRaisesRegex(RuntimeError,'confirmation'):preflight(c)
        c.request.assert_not_called()
        c=Mock();c.hello.return_value={'pending':False};c.request.side_effect=[{}, {}, {'wifi_mode':1,'active_interface':True}]
        with self.assertRaisesRegex(RuntimeError,'disabled'):preflight(c)
        self.assertNotIn(5,[call.args[0] for call in c.request.call_args_list])
    def test_reset_errors_and_failed_trials_are_visible(self):
        before=dict(stats_boot=1,tx_frames=2,tx_bytes=200,tx_airtime_us=300,tx_errors=0)
        after=dict(stats_boot=1,tx_frames=4,tx_bytes=400,tx_airtime_us=600,tx_errors=0)
        self.assertEqual(counter_delta(before,after,'stats_boot')['tx_frames'],2)
        after['stats_boot']=2
        with self.assertRaisesRegex(RuntimeError,'restarted'):counter_delta(before,after,'stats_boot')
        result=summarize([dict(block_bytes=32,success=False,error='lost link')])
        self.assertEqual(result,[dict(block_bytes=32,attempted=1,successful=0)])

if __name__=='__main__':unittest.main()

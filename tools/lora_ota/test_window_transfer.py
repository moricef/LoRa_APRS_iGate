import unittest
from unittest.mock import Mock
from window_transfer import transfer,START,DATA,QUERY,FINISH,PAGE


class WindowClientTests(unittest.TestCase):
    def test_resume_skips_durable_page_and_resends_only_one_hole(self):
        client=Mock(retries=0)
        client.request.side_effect=[
            {'written':PAGE},
            {'written':PAGE,'bitmap':0xffffffff^(1<<9)},
            {'written':2*PAGE,'bitmap':0xffffffff},
            {'written':2*PAGE,'running':0x10000,'boot':0x10000},
        ]
        image=bytes(2*PAGE);events=[]
        transfer(client,image,bytes(32),lambda kind,**kw:events.append((kind,kw)))
        offsets=[call.args[1] for call in client.send_unacknowledged.call_args_list]
        self.assertEqual(offsets,list(range(PAGE,2*PAGE,128))+[PAGE+9*128])
        self.assertEqual([call.args[0] for call in client.request.call_args_list],[START,QUERY,QUERY,FINISH])
        self.assertEqual(next(kw['missing'] for kind,kw in events if kind=='repair_page'),1<<9)

    def test_complete_checkpoint_needs_no_data(self):
        client=Mock(retries=0)
        client.request.side_effect=[{'written':73},{'written':73}]
        transfer(client,bytes(73),bytes(32),lambda *a,**kw:None)
        client.send_unacknowledged.assert_not_called()

    def test_final_short_page_rejects_bitmap_outside_image(self):
        client=Mock(retries=0)
        client.request.side_effect=[{'written':0},{'written':0,'bitmap':2}]
        with self.assertRaisesRegex(RuntimeError,'invalid page bitmap'):
            transfer(client,bytes(73),bytes(32),lambda *a,**kw:None)

    def test_invalid_resume_offset_is_rejected(self):
        client=Mock(retries=0);client.request.return_value={'written':128}
        with self.assertRaisesRegex(RuntimeError,'invalid durable'):
            transfer(client,bytes(PAGE),bytes(32),lambda *a,**kw:None)

    def test_lost_reply_recovers_session_without_resending_committed_page(self):
        client=Mock(retries=0)
        client.request.side_effect=[{'written':0},TimeoutError('relay lost'),
                                    {'written':PAGE},{'written':PAGE}]
        events=[]
        transfer(client,bytes(PAGE),bytes(32),lambda kind,**kw:events.append((kind,kw)))
        client.recover_fast_session.assert_called_once_with()
        self.assertEqual(client.send_unacknowledged.call_count,32)
        self.assertIn(('link_recovered',{'written':PAGE}),events)

    def test_progress_regression_after_recovery_is_rejected(self):
        client=Mock(retries=0)
        client.request.side_effect=[{'written':PAGE},TimeoutError('relay lost'),{'written':0}]
        with self.assertRaisesRegex(RuntimeError,'unexpected durable offset'):
            transfer(client,bytes(2*PAGE),bytes(32),lambda *a,**kw:None)


if __name__=='__main__':unittest.main()

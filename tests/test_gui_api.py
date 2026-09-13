from pathlib import Path
import sys, unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'gui'))
from app import Api
class FakeBridge:
    def __init__(self,state): self.state=state
    def call(self,*args): return self.state
class ApiTests(unittest.TestCase):
    def test_host_restart_preserves_photo_but_clears_old_matches(self):
        api=Api(1);image={'width':750,'height':750};api._image=image
        api._accept({'session':'old','capture_id':1});api._points={'P1':[1,2]}
        target=FakeBridge({'session':'new','capture_id':1});target.host=2
        with patch('app.Bridge',return_value=target),patch('app.available_hosts',return_value=[2]):
            api._activate_host(2)
        self.assertIs(image,api._image);self.assertEqual({},api._points)
        self.assertIs(target,api._bridge)

    def test_same_host_activation_preserves_matches(self):
        api=Api(1);api._points={'P1':[1,2]}
        self.assertIsNone(api._activate_host(1));self.assertEqual({'P1':[1,2]},api._points)

    def test_failed_restore_does_not_switch_host(self):
        api=Api(1);api._points={'P1':[1,2]}
        target=FakeBridge({'session':'new','capture_id':1})
        def failing(command,*args):
            if command=='restore':raise RuntimeError('restore failed')
            return {'session':'old','captured':True}
        with patch('app.Bridge',return_value=target),patch('app.available_hosts',return_value=[1,2]),patch.object(api._bridge,'call',side_effect=failing):
            with self.assertRaisesRegex(RuntimeError,'restore failed'):api._activate_host(2)
        self.assertEqual(1,api._bridge.host);self.assertEqual({'P1':[1,2]},api._points)

    def test_new_capture_clears_matches(self):
        api=Api();api._accept({'session':'a','capture_id':1});api._points={'P1':[1,2]}
        api._accept({'session':'a','capture_id':2});self.assertEqual({},api._points)

    def test_restore_response_must_confirm_restored_state(self):
        api=Api(1)
        failed={'session':'old','capture_id':1,'captured':True,'restored':False}
        api._bridge=FakeBridge(failed)
        self.assertFalse(api.call('restore','old')['ok'])
        target=FakeBridge({'session':'new','capture_id':1})
        old=api._bridge;old.host=1
        with patch('app.Bridge',return_value=target),patch('app.available_hosts',return_value=[1,2]):
            with self.assertRaises(ValueError):api._activate_host(2)
        self.assertIs(old,api._bridge)
    def test_document_switch_rejects_late_photo_click(self):
        api=Api();api._image={'width':750,'height':750}
        api._accept({'session':'a','capture_id':1})
        api._bridge=FakeBridge({'session':'b','capture_id':1,'captured':True,'points':[{'id':'P1'}]})
        result=api.set_point('a',1,'P1',10,10)
        self.assertFalse(result['ok']);self.assertEqual({},api._points)
    def test_current_point_is_persisted(self):
        state={'session':'a','capture_id':1,'captured':True,'points':[{'id':'P1'}]}
        api=Api();api._image={'width':750,'height':750};api._bridge=FakeBridge(state);api._accept(state)
        self.assertTrue(api.set_point('a',1,'P1',20.5,30.25)['ok'])
        self.assertEqual([20.5,30.25],api._points['P1'])
if __name__=='__main__':unittest.main()

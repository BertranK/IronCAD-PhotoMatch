from pathlib import Path
import sys, unittest, json, tempfile
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'gui'))
from app import Api
from project import load_image, make_project
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

    def test_photo_point_can_be_edited_after_camera_restore(self):
        state={'session':'a','capture_id':1,'captured':False,'restored':True,'points':[{'id':'P1'},{'id':'P2'}]}
        api=Api();api._image={'width':750,'height':750};api._bridge=FakeBridge(state);api._accept(state)
        api._points={'P1':[10,20],'P2':[50,60]}
        self.assertTrue(api.set_point('a',1,'P1',21,31)['ok'])
        self.assertEqual({'P1':[21,31],'P2':[50,60]},api._points)
        self.assertFalse(api.set_point('a',0,'P1',1,1)['ok'])

    def test_reopen_results_preserves_restored_session_and_matches(self):
        state={'session':'a','capture_id':1,'captured':False,'points':[{'id':'P1','vertex_id':10}]}
        image=load_image(Path(__file__).parent/'fixture/reference.png')
        data=make_project(state,image,{'P1':[12,34]})
        api=Api();api._bridge=FakeBridge(state)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'result.json';path.write_text(json.dumps(data),encoding='utf-8')
            result=api._load_project(path)
            self.assertEqual({'P1':[12,34]},result['image_points'])
            self.assertFalse(result['state']['captured'])
            self.assertEqual(image['sha256'],result['image']['sha256'])
            for change in ['photo','vertex']:
                bad=json.loads(json.dumps(data))
                if change=='session':bad['host']['session']='old'
                elif change=='photo':bad['image']['sha256']='changed'
                else:bad['host']['points'][0]['vertex_id']=99
                path.write_text(json.dumps(bad),encoding='utf-8')
                with self.assertRaises(ValueError):api._load_project(path)
                self.assertEqual({'P1':[12,34]},api._points)
    def test_old_session_reopens_for_photo_editing_without_host_mutation(self):
        saved={'session':'old','capture_id':1,'points':[{'id':'P1','vertex_id':10}]}
        current={'session':'new','capture_id':0,'points':[]}
        image=load_image(Path(__file__).parent/'fixture/reference.png')
        data=make_project(saved,image,{'P1':[12,34]})
        api=Api();api._bridge=FakeBridge(current)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'result.json';path.write_text(json.dumps(data),encoding='utf-8')
            result=api._load_project(path)
            self.assertEqual(current,result['state'])
            self.assertEqual(saved,result['review'])
            self.assertTrue(api.set_point('new',0,'P1',22,44)['ok'])
            with patch.object(api._bridge,'call',side_effect=AssertionError('must not mutate host')):
                for command in ['capture','apply','pick','photo','background','measure']:
                    self.assertFalse(api.call(command,'new')['ok'])
            self.assertEqual({'P1':[22,44]},api.call('status')['image_points'])
    def test_reopen_reconnects_only_matching_document_with_host_verification(self):
        saved={'session':'old','capture_id':1,'document':'test.ics','points':[{'id':'P1','vertex_id':10}]}
        current={'session':'new','capture_id':0,'document':'test.ics','points':[],'saved_point_reconnect':True}
        rebound={**current,'capture_id':1,'points':saved['points'],'captured':True}
        image=load_image(Path(__file__).parent/'fixture/reference.png');data=make_project(saved,image,{'P1':[12,34]})
        api=Api();api._bridge=FakeBridge(current)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'result.json';path.write_text(json.dumps(data),encoding='utf-8')
            with patch.object(api._bridge,'call',side_effect=[current,rebound]) as bridge:
                result=api._load_project(path)
                self.assertIsNone(result['review']);self.assertEqual({'P1':[12,34]},result['image_points'])
                self.assertEqual(('reconnect','new',{'host':saved}),bridge.call_args.args)
            with patch.object(api._bridge,'call',side_effect=[current,ValueError('changed vertex')]):
                with self.assertRaisesRegex(ValueError,'changed vertex'):api._load_project(path)
                self.assertEqual({'P1':[12,34]},api._points)
    def fitted_api(self):
        state={'session':'a','capture_id':1,'points':[{'id':f'P{i+1}','transformed_coordinates':[i%2,i//2,i%3]} for i in range(6)]}
        api=Api();api._bridge=FakeBridge(state);api._accept(state)
        api._image={'width':750,'height':750,'overlay_path':'photo.png'}
        api._points={p['id']:[100+i*20,200] for i,p in enumerate(state['points'])}
        fit={'stable':True,'camera':{},'focal_px':1000,'max_error_px':3.4,'precision_passed':False}
        with patch('solver.fit_camera',return_value=fit):self.assertTrue(api.fit_points()['ok'])
        return api,state

    def test_fit_invalidated_by_edit_or_changed_model(self):
        api,state=self.fitted_api()
        self.assertFalse(api._fit['precision_passed'])
        self.assertTrue(api.set_point('a',1,'P1',101,200)['ok']);self.assertIsNone(api._fit)
        api,state=self.fitted_api()
        state['points'][0]['transformed_coordinates'][0]=99
        self.assertIsNone(api.call('status')['fit'])

    def test_slow_fit_cannot_attach_to_changed_capture(self):
        api,state=self.fitted_api()
        def fit(*args):
            state['capture_id']=2
            return {'stable':True}
        with patch('solver.fit_camera',side_effect=fit):self.assertFalse(api.fit_points()['ok'])
        self.assertIsNone(api._fit);self.assertEqual({},api._points)

    def test_preview_measures_actual_screen_error_and_review_never_applies(self):
        api,state=self.fitted_api();commands=[]
        def bridge(command,*args):
            commands.append(command)
            if command=='measure':
                state['photo_rectangle_physical']=[10,20,1500,1500]
                state['viewport']=state['photo_render_size']=[1600,1600]
                state['projection_coordinate_rule']='sdk_pixel_endpoints_truncate_then_physical_scale'
                state['measurements']=[{'picked_point_projections':[
                    {'id':id,'transformed_as_world_px':[10+xy[0]*2+2,20+xy[1]*2]} for id,xy in api._points.items()]}]
            return state
        with patch.object(api._bridge,'call',side_effect=bridge):
            result=api.preview_fit()
            self.assertTrue(result['ok'],result)
            self.assertEqual(2,result['fit']['screen_max_error_px']);self.assertFalse(result['fit']['screen_passed'])
            self.assertEqual(['status','apply','measure','photo','measure'],commands)
            commands.clear();api._review=state.copy()
            self.assertFalse(api.preview_fit()['ok']);self.assertEqual(['status'],commands)

    def test_screen_comparison_preserves_raw_error_and_physical_pixel_gate(self):
        api,state=self.fitted_api()
        state['photo_rectangle_physical']=[.9,.9,750,750]
        state['viewport']=state['photo_render_size']=[1000,1000]
        state['projection_coordinate_rule']='sdk_pixel_endpoints_truncate_then_physical_scale'
        api._points={id:[100,200] for id in api._fit['ids']}
        state['measurements']=[{'picked_point_projections':[
            {'id':id,'transformed_as_world_px':[100,200]} for id in api._fit['ids']]}]
        with patch.object(api._bridge,'call',return_value=state):
            result=api.preview_fit()
            self.assertTrue(result['ok'],result)
            self.assertGreater(result['fit']['screen_max_error_px'],1)
            self.assertFalse(result['fit']['screen_passed'])
            self.assertEqual(0,result['fit']['sdk_raster_max_error_px'])
            self.assertTrue(result['fit']['sdk_raster_passed'])
            # A one-render-pixel displacement is 1.5 physical pixels here and
            # must still fail; DPI virtualization does not widen the threshold.
            state['viewport']=[1500,1500]
            for row in state['measurements'][0]['picked_point_projections']:
                row['transformed_as_world_px']=[100.5,201]
            result=api.preview_fit()
            self.assertFalse(result['fit']['sdk_raster_passed'])

if __name__=='__main__':unittest.main()

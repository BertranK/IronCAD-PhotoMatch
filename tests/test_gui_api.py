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
    def test_preview_toggle_preserves_camera_fit_points_and_opacity(self):
        api,state=self.fitted_api();state.update(photo_preview_enabled=True,photo_opacity=.25,camera={'field':1.2})
        original_fit=api._fit;original_points=api._points.copy();camera=state['camera'].copy()
        for enabled in [False,True]:
            with patch.object(api._bridge,'call',return_value={**state,'photo_preview_enabled':enabled}) as call:
                result=api.call('photo_preview','a',{'enabled':enabled})
            self.assertTrue(result['ok'],result.get('error'))
            call.assert_called_once_with('photo_preview','a',{'enabled':enabled})
            self.assertEqual(enabled,result['state']['photo_preview_enabled']);self.assertEqual(.25,result['state']['photo_opacity'])
            self.assertEqual(camera,result['state']['camera']);self.assertIs(original_fit,result['fit'])
            self.assertEqual(original_points,result['image_points'])

    def test_window_close_hides_markers_after_picking_or_restoring_without_deleting_points(self):
        for captured in [True,False]:
            state={'session':'a','capture_id':1,'captured':captured,'restored':not captured,
                   'photo_workflow_version':1,'points':[{'id':'P1'}]}
            closed={**state,'captured':False,'restored':True}
            api=Api(42);api._accept(state);api._image={'path':'photo.png'};api._points={'P1':[12,34]}
            with patch('app.available_hosts',return_value=[42]), patch.object(api._bridge,'call',side_effect=[state,closed]) as call:
                self.assertTrue(api._closing())
            self.assertEqual([unittest.mock.call('status'),unittest.mock.call('close_photo','a')],call.call_args_list)
            self.assertEqual([{'id':'P1'}],api._state['points'])
            self.assertEqual({'P1':[12,34]},api._points);self.assertEqual({'path':'photo.png'},api._image)

    def test_window_close_skips_disconnected_host_and_other_document(self):
        api=Api(42);api._accept({'session':'a','capture_id':1,'captured':True,'points':[{'id':'P1'}]})
        with patch('app.available_hosts',return_value=[]), patch.object(api._bridge,'call') as call:
            self.assertTrue(api._closing());call.assert_not_called()
        other={'session':'b','capture_id':2,'captured':True,'photo_workflow_version':1,'points':[{'id':'P9'}]}
        with patch('app.available_hosts',return_value=[42]), patch.object(api._bridge,'call',return_value=other) as call:
            self.assertTrue(api._closing())
        self.assertEqual([unittest.mock.call('status')],call.call_args_list)

    def test_window_close_failure_keeps_window_and_correspondences_for_retry(self):
        for captured in [True,False]:
            state={'session':'a','capture_id':1,'captured':captured,'photo_workflow_version':1,'points':[{'id':'P1'}]}
            api=Api(42);api._accept(state);api._points={'P1':[12,34]};api._window=unittest.mock.Mock()
            with patch('app.available_hosts',return_value=[42]), patch.object(api._bridge,'call',side_effect=[state,RuntimeError('cleanup failed')]):
                self.assertFalse(api._closing())
            api._window.evaluate_js.assert_called_once_with('window.closeError("cleanup failed")')
            self.assertEqual({'P1':[12,34]},api._points)

    def test_window_close_older_host_still_restores_camera(self):
        state={'session':'a','capture_id':1,'captured':True,'points':[]}
        api=Api(42);api._accept(state)
        with patch('app.available_hosts',return_value=[42]), patch.object(api._bridge,'call',side_effect=[state,{**state,'captured':False,'restored':True}]) as call:
            self.assertTrue(api._closing())
        self.assertEqual(('restore','a'),call.call_args.args)

    def test_reconnect_saved_scene_at_new_path_verifies_references_before_accepting(self):
        saved={'session':'old','capture_id':1,'document':r'C:\Temp\brick.ics','points':[{'id':'P1','vertex_id':10}]}
        current={'session':'new','capture_id':0,'document':r'C:\Models\brick.ics','points':[],'saved_point_reconnect':True}
        rebound={**current,'capture_id':1,'points':saved['points']}
        for response in [rebound,ValueError('Saved vertex geometry changed')]:
            api=Api();api._accept(current);api._review=saved;api._points={'P1':[12,34]}
            with patch.object(api._bridge,'call',side_effect=[current,response]) as call:
                result=api.reconnect_model()
            self.assertEqual(('reconnect','new',{'host':{'document':saved['document'],'points':saved['points']}}),call.call_args.args)
            self.assertEqual({'P1':[12,34]},api._points)
            if isinstance(response,Exception):
                self.assertFalse(result['ok']);self.assertEqual(saved,api._review)
            else:
                self.assertTrue(result['ok']);self.assertIsNone(result['review'])

    def test_open_relocated_scene_reconnects_after_native_verification(self):
        image=load_image(Path(__file__).parent/'fixture/reference.png')
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'match.json';scene=Path(directory)/'brick.ics';scene.write_bytes(b'fixture')
            saved={'session':'old','capture_id':1,'document':r'C:\Temp\brick.ics','points':[{'id':'P1','vertex_id':10}]}
            data=make_project(saved,image,{'P1':[12,34]});data['scene_path']=str(scene)
            path.write_text(json.dumps(data),encoding='utf-8')
            current={'session':'new','capture_id':0,'document':str(scene),'points':[],'scene_project_supported':True,'saved_point_reconnect':True}
            rebound={**current,'capture_id':1,'points':saved['points']}
            api=Api()
            with patch.object(api._bridge,'call',side_effect=[current,rebound]) as call:
                result=api._load_project(path)
            self.assertEqual('reconnect',call.call_args.args[0]);self.assertIsNone(result['review'])
            self.assertEqual({'P1':[12,34]},result['image_points'])

    def test_scene_open_waits_five_minutes_and_restores_normal_timeout(self):
        for command,timeout in [('open_scene',300000),('save_scene',60000)]:
            for fail in [False,True]:
                api=Api();normal=api._bridge.timeout_ms
                def bridge(*args):
                    self.assertEqual(timeout,api._bridge.timeout_ms)
                    if fail: raise RuntimeError('scene failed')
                    return {'ok':True}
                with patch.object(api._bridge,'call',side_effect=bridge):
                    if fail:
                        with self.assertRaisesRegex(RuntimeError,'scene failed'):api._scene_call(command,'a',{})
                    else:self.assertEqual({'ok':True},api._scene_call(command,'a',{}))
                self.assertEqual(normal,api._bridge.timeout_ms)

    def test_missing_temp_scene_uses_adjacent_or_open_saved_scene_with_verification(self):
        image=load_image(Path(__file__).parent/'fixture/reference.png')
        for adjacent in [True,False]:
            with tempfile.TemporaryDirectory() as directory:
                folder=Path(directory);path=folder/'match.json';scene=folder/('brick.ics' if adjacent else 'renamed.ics');scene.write_bytes(b'fixture')
                missing=folder/'deleted-temp'/'brick.ics'
                saved={'session':'old','capture_id':1,'document':str(missing),'points':[{'id':'P1','vertex_id':10}]}
                data=make_project(saved,image,{'P1':[12,34]});data['scene_path']=str(missing)
                path.write_text(json.dumps(data),encoding='utf-8')
                current={'session':'new','capture_id':0,'document':str(scene),'points':[],'scene_project_supported':True,'saved_point_reconnect':True}
                rebound={**current,'capture_id':1,'points':saved['points']}
                for response in [rebound,ValueError('Saved vertex geometry changed')]:
                    api=Api();api._image={'path':'keep.png'};api._points={'P9':[20,30]}
                    with patch.object(api._bridge,'call',side_effect=[current,response]) as call:
                        if isinstance(response,Exception):
                            with self.assertRaisesRegex(ValueError,'geometry changed'):api._load_project(path)
                            self.assertEqual({'P9':[20,30]},api._points);self.assertEqual('keep.png',api._image['path'])
                        else:
                            result=api._load_project(path);self.assertIsNone(result['review']);self.assertEqual({'P1':[12,34]},result['image_points'])
                    self.assertEqual('reconnect',call.call_args.args[0])

    def test_clear_all_removes_pairs_in_one_host_call_and_keeps_photo(self):
        state={'session':'a','capture_id':1,'point_clear_supported':True,'points':[{'id':'P1'},{'id':'P8'}]}
        cleared={**state,'capture_id':2,'points':[]}
        api=Api();api._accept(state);api._points={'P1':[10,20]};api._image={'path':'photo.png'}
        with patch.object(api._bridge,'call',side_effect=[state,cleared]) as call:
            result=api.clear_points('a',1,True)
        self.assertTrue(result['ok']);self.assertEqual(('clear_points','a',{'capture_id':1}),call.call_args.args)
        self.assertEqual([],result['state']['points']);self.assertEqual({},result['image_points']);self.assertIsNone(result['review'])
        self.assertEqual(('a',2),api._key);self.assertEqual('photo.png',api._image['path'])

    def test_clear_all_rejects_stale_adjusting_old_host_and_native_failure(self):
        state={'session':'a','capture_id':1,'point_clear_supported':True,'points':[{'id':'P8'}]}
        for current,session,capture,error in [(state,'a',0,None),(state,'old',1,None),
                ({**state,'adjusting_camera':True},'a',1,None),({**state,'point_clear_supported':False},'a',1,None),
                (state,'a',1,RuntimeError('restore failed'))]:
            api=Api();api._accept(current);api._points={'P8':[10,20]}
            with patch.object(api._bridge,'call',side_effect=[current,error] if error else [current]) as call:
                self.assertFalse(api.clear_points(session,capture,True)['ok'])
            self.assertEqual(2 if error else 1,call.call_count);self.assertEqual({'P8':[10,20]},api._points)

    def test_clear_all_review_does_not_delete_unrelated_active_model_points(self):
        state={'session':'new','capture_id':2,'points':[{'id':'P9'}]}
        api=Api();api._accept(state);api._review={'document':'old.ics','points':[{'id':'P1'}]};api._points={'P1':[10,20]}
        with patch.object(api._bridge,'call',return_value=state) as call:result=api.clear_points('new',2,True)
        self.assertTrue(result['ok']);self.assertEqual(1,call.call_count);self.assertEqual([],result['review']['points'])
        self.assertEqual([{'id':'P9'}],result['state']['points']);self.assertEqual({},result['image_points'])

    def test_delete_all_pairs_then_pick_and_match_six_again(self):
        import copy
        api=Api();image=load_image(Path(__file__).parent/'fixture/reference.png');api._image=image
        world=[[0,0,0],[1,0,0],[0,1,0],[1,1,1],[0,0,2],[1,2,1]]
        rows=[{'id':f'P{i+1}','binding_status':'connected','transformed_coordinates':p} for i,p in enumerate(world)]
        state={'session':'a','capture_id':1,'document':'test.ics','point_delete_supported':True,'points':copy.deepcopy(rows)}
        def bridge(command,session='',args=None):
            if command=='delete_point':
                state['points']=[p for p in state['points'] if p['id']!=args['id']]
                if not state['points']: state['capture_id']+=1
            return copy.deepcopy(state)
        with patch.object(api._bridge,'call',side_effect=bridge):
            api._accept(copy.deepcopy(state))
            for cycle in range(2):
                for row in rows:
                    x,y,z=row['transformed_coordinates']
                    result=api.set_point('a',state['capture_id'],row['id'],image['width']/2+100*(x+.3)/(z+5),image['height']/2+100*(y+.2)/(z+5))
                    self.assertTrue(result['ok'],result.get('error'))
                result=api.fit_points()
                self.assertTrue(result['ok'],result.get('error'))
                self.assertEqual([p['id'] for p in rows],result['fit']['ids'])
                if cycle==0:
                    for row in rows:
                        result=api.delete_point('a',state['capture_id'],row['id'])
                        self.assertTrue(result['ok'],result.get('error'))
                    self.assertEqual({},result['image_points']);self.assertIsNone(result['review']);self.assertIsNone(result['fit'])
                    self.assertIs(api._image,image)
                    state['points']=copy.deepcopy(rows)
                    api._accept(copy.deepcopy(state))

    def test_first_photo_opens_without_host_but_active_preview_is_preserved_on_failure(self):
        from unittest.mock import Mock
        api=Api();api._window=Mock()
        api._window.create_file_dialog.return_value=[str(Path(__file__).parent/'fixture/reference.png')]
        with patch.object(api._bridge,'call',side_effect=RuntimeError('Host unavailable')) as call:
            self.assertTrue(api.open_image()['ok'])
            call.assert_not_called()
            original=api._image
            api._preview_session='a';api._points={'P1':[12,34]}
            self.assertFalse(api.open_image()['ok'])
            self.assertIs(original,api._image)
            self.assertEqual({'P1':[12,34]},api._points)

    def test_delete_point_preserves_remaining_ids_pixels_and_saved_rows(self):
        state={'session':'a','capture_id':1,'point_delete_supported':True,
               'points':[{'id':'P1'},{'id':'P2'},{'id':'P3'}]}
        api=Api();api._accept(state);api._points={'P1':[10,20],'P2':[30,40],'P3':[50,60]}
        api._fit={'host_points':state['points']};api._adjustment_fit={'old':True}
        updated={**state,'points':[state['points'][0],state['points'][2]]}
        with patch.object(api._bridge,'call',side_effect=[state,updated]) as call:
            result=api.delete_point('a',1,'P2')
        self.assertTrue(result['ok']);self.assertEqual(['P1','P3'],[p['id'] for p in result['state']['points']])
        self.assertEqual({'P1':[10,20],'P3':[50,60]},result['image_points'])
        self.assertIsNone(result['fit']);self.assertIsNone(api._adjustment_fit)
        self.assertEqual(('delete_point','a',{'id':'P2','capture_id':1}),call.call_args.args)
        saved=make_project(result['state'],{'width':100,'height':100},result['image_points'])
        self.assertEqual(['P1','P3'],[p['id'] for p in saved['correspondences']])

    def test_delete_point_rejects_stale_unknown_adjusting_and_old_hosts(self):
        base={'session':'a','capture_id':1,'point_delete_supported':True,'points':[{'id':'P1'}]}
        for state,session,capture,point in [(base,'old',1,'P1'),(base,'a',0,'P1'),
                (base,'a',1,'P9'),({**base,'adjusting_camera':True},'a',1,'P1'),
                ({**base,'point_delete_supported':False},'a',1,'P1')]:
            with self.subTest(state=state,session=session,capture=capture,point=point):
                api=Api();api._accept(state);api._points={'P1':[10,20]}
                with patch.object(api._bridge,'call',return_value=state) as call:
                    self.assertFalse(api.delete_point(session,capture,point)['ok'])
                self.assertEqual({'P1':[10,20]},api._points)
                self.assertEqual(1,call.call_count)

    def test_delete_failure_preserves_photo_point(self):
        state={'session':'a','capture_id':1,'point_delete_supported':True,'points':[{'id':'P1'}]}
        api=Api();api._accept(state);api._points={'P1':[10,20]}
        with patch.object(api._bridge,'call',side_effect=[state,RuntimeError('delete failed')]):
            self.assertFalse(api.delete_point('a',1,'P1')['ok'])
        self.assertEqual({'P1':[10,20]},api._points)

    def test_delete_review_point_does_not_change_current_model(self):
        state={'session':'new','capture_id':2,'points':[{'id':'P9'}]}
        api=Api();api._accept(state)
        api._review={'document':'old.ics','points':[{'id':'P1'},{'id':'P3'}]}
        api._points={'P1':[10,20],'P3':[50,60]}
        with patch.object(api._bridge,'call',return_value=state) as call:
            result=api.delete_point('new',2,'P1')
        self.assertTrue(result['ok']);self.assertEqual(1,call.call_count)
        self.assertEqual([{'id':'P3'}],result['review']['points'])
        self.assertEqual([{'id':'P9'}],result['state']['points'])
        self.assertEqual({'P3':[50,60]},result['image_points'])

    def test_replace_model_preserves_photo_pixels_and_requires_new_vertex_bindings(self):
        api=Api();image=load_image(Path(__file__).parent/'fixture/reference.png');api._image=image
        old={'session':'old','capture_id':1,'document':'old.ics','points':[{'id':'P1'},{'id':'P2'}]}
        api._accept(old);api._points={'P1':[12.25,34.5],'P2':[45,67]}
        current={'session':'new','capture_id':2,'document':'new.ics','points':[],'replace_model_supported':True}
        api._accept(current);saved=api._review.copy();commands=[]
        pending=[{'id':p['id'],'binding_status':'needs_reconnection','api_coordinates':[0,0,0],
                  'transformed_coordinates':[0,0,0]} for p in old['points']]
        replacement={**current,'capture_id':3,'captured':True,'points':pending}
        def bridge(command,*args):
            commands.append((command,*args))
            return replacement if command=='replace_model' else current
        with patch.object(api._bridge,'call',side_effect=bridge):
            self.assertFalse(api.replace_model('old',1)['ok'])
            self.assertEqual(saved,api._review)
            result=api.replace_model('new',2)
        self.assertTrue(result['ok']);self.assertIsNone(api._review);self.assertIs(image,api._image)
        self.assertEqual({'P1':[12.25,34.5],'P2':[45,67]},result['image_points'])
        self.assertEqual(('replace_model','new',{'ids':['P1','P2']}),commands[-1])
        self.assertIsNone(result['fit']);self.assertEqual(pending,result['state']['points'])
        with patch.object(api._bridge,'call',return_value=replacement):
            self.assertFalse(api.fit_points()['ok'])
        # Incomplete replacement results retain every photo row through save/reopen.
        data=make_project(replacement,image,api._points)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'replacement.json';path.write_text(json.dumps(data),encoding='utf-8')
            reopened=Api();reopened._bridge=FakeBridge(replacement)
            self.assertEqual(api._points,reopened._load_project(path)['image_points'])

    def test_replace_model_failure_keeps_review_and_pixels(self):
        api=Api();api._image={'width':750,'height':750};api._points={'P1':[12,34]}
        state={'session':'new','capture_id':0,'document':'new.ics','points':[],'replace_model_supported':True}
        api._accept(state);api._review={'document':'old.ics','points':[{'id':'P1'}]}
        for error in [RuntimeError('capture failed'),RuntimeError('document changed')]:
            with patch.object(api._bridge,'call',side_effect=[state,error]):
                self.assertFalse(api.replace_model('new',0)['ok'])
            self.assertEqual('old.ics',api._review['document']);self.assertEqual({'P1':[12,34]},api._points)

    def test_host_restart_preserves_photo_and_keeps_old_matches_in_review(self):
        api=Api(1);image={'width':750,'height':750};api._image=image
        api._accept({'session':'old','capture_id':1});api._points={'P1':[1,2]}
        target=FakeBridge({'session':'new','capture_id':1});target.host=2
        with patch('app.Bridge',return_value=target),patch('app.available_hosts',return_value=[2]):
            api._activate_host(2)
        self.assertIs(image,api._image);self.assertEqual({'P1':[1,2]},api._points)
        self.assertEqual('old',api._review['session'])
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

    def test_new_capture_preserves_photo_matches(self):
        api=Api();api._accept({'session':'a','capture_id':1});api._points={'P1':[1,2]}
        api._accept({'session':'a','capture_id':2});self.assertEqual({'P1':[1,2]},api._points)

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
                self.assertEqual(('reconnect','new',{'host':{'document':saved['document'],'points':saved['points']}}),bridge.call_args.args)
            with patch.object(api._bridge,'call',side_effect=[current,ValueError('changed vertex')]):
                with self.assertRaisesRegex(ValueError,'changed vertex'):api._load_project(path)
                self.assertEqual({'P1':[12,34]},api._points)
    def test_large_saved_diagnostics_do_not_overflow_project_reconnect_request(self):
        point={'id':'P1','object_id':'42','vertex_id':10,'api_coordinates':[1,2,3],
               'transformed_coordinates':[1,2,3],'transform':[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],
               'binding_status':'verified','object_name':'model'}
        model_points=[{**point,'id':f'P{i+1}','vertex_id':10+i} for i in range(60)]
        saved={'session':'old','capture_id':1,'document':'test.ics','points':model_points,
               'measurements':[{'diagnostic':'x'*70000}],'logs':['previous measurements']}
        current={'session':'new','capture_id':0,'document':'test.ics','points':[],
                 'saved_point_reconnect':True}
        rebound={**current,'capture_id':1,'points':model_points}
        image=load_image(Path(__file__).parent/'fixture/reference.png')
        data=make_project(saved,image,{'P1':[12,34]})
        self.assertGreater(len(json.dumps({'host':saved}).encode('utf-8')),65536)
        requests=[]
        def bridge(command,session='',args=None):
            if command=='status': return current
            encoded=json.dumps({'command':command,'session':session,'args':args}).encode('utf-8')
            if len(encoded)>65536: raise ValueError('Request is too large')
            requests.append(args['host'])
            return rebound
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'large.json';original=json.dumps(data);path.write_text(original,encoding='utf-8')
            api=Api()
            with patch.object(api._bridge,'call',side_effect=bridge):
                opened=api._load_project(path)
                self.assertIsNone(opened['review'])
                self.assertEqual({'P1':[12,34]},opened['image_points'])
                api._review=saved
                self.assertTrue(api.reconnect_model()['ok'])
            self.assertEqual(original,path.read_text(encoding='utf-8'))
        self.assertEqual(2,len(requests))
        for host in requests:
            self.assertEqual('test.ics',host['document'])
            self.assertEqual([{k:v for k,v in p.items() if k!='object_name'} for p in model_points],host['points'])
            self.assertEqual({'document','points'},set(host))

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

    def test_model_edit_and_deleted_vertex_preserve_all_photo_points(self):
        api,state=self.fitted_api();original=dict(api._points)
        state['points'][1]['transformed_coordinates']=[9,8,7]
        state['points'][3]['binding_status']='needs_reconnection'
        result=api.call('status')
        self.assertEqual(original,result['image_points']);self.assertIsNone(result['fit'])
        self.assertFalse(api.fit_points()['ok'])
        self.assertEqual(1,sum(p.get('binding_status')=='needs_reconnection' for p in result['state']['points']))
        self.assertTrue(api.set_point('a',1,'P4',123,234)['ok'])

    def test_close_photo_restores_only_its_owned_capture_and_failure_retains_input(self):
        api,state=self.fitted_api();state.update(captured=True,photo_workflow_version=1)
        api._preview_session='a';original=dict(api._points);commands=[]
        def bridge(command,*args):
            commands.append(command)
            if command=='close_photo':state.update(captured=False,restored=True)
            return state
        with patch.object(api._bridge,'call',side_effect=bridge):
            result=api.close_image()
        self.assertTrue(result['ok']);self.assertIsNone(api._image);self.assertEqual({},api._points)
        self.assertEqual(['status','close_photo'],commands);self.assertEqual(6,len(state['points']))
        api,state=self.fitted_api();state.update(captured=True);api._preview_session='a'
        with patch.object(api._bridge,'call',side_effect=[state,RuntimeError('restore failed')]):
            self.assertFalse(api.close_image()['ok'])
        self.assertIsNotNone(api._image);self.assertEqual(original,api._points)

    def test_photo_close_after_document_switch_never_restores_other_document(self):
        api,state=self.fitted_api();api._preview_session='a';state['document']='first.ics'
        api._accept(state)
        other={'session':'b','capture_id':4,'document':'second.ics','captured':True,'points':[]}
        with patch.object(api._bridge,'call',return_value=other) as bridge:
            result=api.close_image()
        self.assertTrue(result['ok']);self.assertEqual([unittest.mock.call('status')],bridge.call_args_list)
        self.assertTrue(other['captured'])

    def test_manual_adjustment_cancels_to_previous_fit_and_saves_actual_camera(self):
        api,state=self.fitted_api();api._preview_session='a'
        state.update(photo_workflow_version=1,image_focal_px=1000,photo_principal_px=[375,375],
                     camera={'pose_and_field':{'position':[1,2,3]}},viewport=[1000,800])
        commands=[]
        def bridge(command,session='',args=None):
            commands.append(command)
            if command=='adjust_camera':
                action=args['action'];state['adjusting_camera']=action=='begin'
                if action=='save':state['manual_camera']=json.loads(json.dumps(state['camera']))
            if command=='measure':state['measurements']=[]
            return state
        with patch.object(api._bridge,'call',side_effect=bridge):
            self.assertTrue(api.adjust_camera('begin')['ok']);self.assertIsNone(api._fit)
            self.assertFalse(api.preview_fit()['ok'])
            self.assertTrue(api.adjust_camera('cancel')['ok']);self.assertEqual(3.4,api._fit['max_error_px'])
            self.assertTrue(api.adjust_camera('begin')['ok'])
            state['camera']['pose_and_field']['position']=[4,5,6]
            result=api.adjust_camera('save')
        self.assertTrue(result['ok'],result);self.assertEqual('manual',result['fit']['source'])
        self.assertEqual([4,5,6],result['fit']['manual_camera_state']['pose_and_field']['position'])
        self.assertIsNone(result['fit']['max_error_px']);self.assertFalse(result['fit']['precision_passed'])
        self.assertNotIn('apply',commands);self.assertNotIn('photo',commands)

    def test_cancel_manual_adjustment_after_resize_invalidates_old_residual(self):
        api,state=self.fitted_api();api._preview_session='a'
        state.update(photo_workflow_version=1,viewport=[1000,800])
        api._fit.update(source='manual', max_error_px=.2, point_errors_px=[.2],
                        predicted_image_px=[[100,200]], precision_passed=True,
                        residual_source='current_frame_verified_floating_projection')
        with patch.object(api._bridge,'call',return_value=state):
            self.assertTrue(api.adjust_camera('begin')['ok'])
            state['viewport']=[800,1000]
            result=api.adjust_camera('cancel')
        self.assertTrue(result['ok']);self.assertEqual('manual',result['fit']['source'])
        self.assertIsNone(result['fit']['max_error_px'])
        self.assertEqual([],result['fit']['predicted_image_px'])
        self.assertEqual([],result['fit']['point_errors_px'])
        self.assertFalse(result['fit']['precision_passed'])
        self.assertNotIn('residual_source',result['fit'])

    def test_shifted_principal_requires_matching_native_module_and_is_forwarded(self):
        api,state=self.fitted_api();api._fit.update(estimate_principal=True,principal_px=[692,-4])
        with patch.object(api._bridge,'call',return_value=state) as bridge:
            self.assertFalse(api.preview_fit()['ok']);self.assertEqual(1,bridge.call_count)
        state.update(photo_workflow_version=1,photo_rectangle_physical=[10,20,750,750],
                     viewport=[1000,1000],photo_render_size=[1000,1000],
                     projection_coordinate_rule='sdk_pixel_endpoints_truncate_then_physical_scale',
                     measurements=[{'picked_point_projections':[{'id':id,'transformed_as_world_px':pixel} for id,pixel in api._points.items()]}])
        with patch.object(api._bridge,'call',return_value=state) as bridge:
            self.assertTrue(api.preview_fit()['ok'])
        photo=[call for call in bridge.call_args_list if call.args[0]=='photo'][0]
        self.assertEqual([692,-4],photo.args[2]['principal_px'])

    def test_camera_change_invalidates_measured_pass(self):
        api,state=self.fitted_api();state.update(camera={'pose_and_field':{'position':[0,0,0]}},viewport=[1000,800])
        api._fit.update(screen_passed=True,screen_max_error_px=0.)
        api._measured_view=json.loads(json.dumps([state['camera'],state['viewport']]))
        state['camera']['pose_and_field']['position']=[1,0,0]
        self.assertNotIn('screen_passed',api.call('status')['fit'])

    def test_slow_fit_cannot_attach_to_changed_capture(self):
        api,state=self.fitted_api()
        def fit(*args, **kwargs):
            state['capture_id']=2
            return {'stable':True}
        with patch('solver.fit_camera',side_effect=fit):self.assertFalse(api.fit_points()['ok'])
        self.assertIsNone(api._fit);self.assertEqual(6,len(api._points))

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

    def test_screen_comparison_uses_committed_draw_bounds(self):
        api,state=self.fitted_api()
        state['photo_rectangle_physical']=[.49,.49,749.6,749.6]
        state['photo_drawn_rectangle_physical']=[0,0,750,750]
        state['viewport']=state['photo_render_size']=[1000,1000]
        state['projection_coordinate_rule']='sdk_pixel_endpoints_truncate_then_physical_scale'
        api._points={id:[100,200] for id in api._fit['ids']}
        state['measurements']=[{'picked_point_projections':[
            {'id':id,'transformed_as_world_px':[100,200]} for id in api._fit['ids']]}]
        with patch.object(api._bridge,'call',return_value=state):
            result=api.preview_fit()
            self.assertTrue(result['ok'],result)
            self.assertEqual(0,result['fit']['screen_max_error_px'])
            self.assertEqual([0,0,750,750],result['fit']['screen_image_rectangle'])
            self.assertTrue(result['fit']['screen_image_rectangle_verified'])

    def test_floating_screen_gate_requires_native_frame_verification(self):
        api,state=self.fitted_api()
        state['photo_rectangle_physical']=state['photo_drawn_rectangle_physical']=[0,0,750,750]
        state['viewport']=state['photo_render_size']=[1000,1000]
        state['projection_coordinate_rule']='sdk_pixel_endpoints_truncate_then_physical_scale'
        api._points={id:[100.9,200.9] for id in api._fit['ids']}
        m={'picked_point_projections':[
            {'id':id,'transformed_as_world_px':[100,200],'model_double_physical_px':[100.9,200.9]} for id in api._fit['ids']]}
        state['measurements']=[m]
        with patch.object(api._bridge,'call',return_value=state):
            r=api.preview_fit()['fit']
            self.assertFalse(r['screen_passed'])
            self.assertGreater(r['integer_to_photo_max_error_px'],1)
            m['floating_world_frame_verified']=True
            r=api.preview_fit()['fit']
            self.assertEqual(0,r['screen_max_error_px'])
            self.assertTrue(r['screen_passed'])
            m['picked_point_projections'][0]['model_double_physical_px'][0]+=2
            self.assertFalse(api.preview_fit()['fit']['screen_passed'])

    def test_clear_points_keeps_model_links_numbers_image_and_saved_rows(self):
        api=Api();state={'session':'a','capture_id':1,'points':[{'id':'P1'},{'id':'P3'},{'id':'P8'}]}
        api._accept(state);api._image={'path':'photo.png'};api._points={'P1':[10,20],'P8':[30,40]}
        with patch.object(api._bridge,'call',return_value=state) as call:
            result=api.clear_points('a',1)
        self.assertEqual(1,call.call_count)
        self.assertTrue(result['ok']);self.assertEqual({},result['image_points'])
        self.assertEqual(state['points'],result['state']['points']);self.assertEqual('photo.png',api._image['path'])
        self.assertIsNone(result['review']);self.assertEqual(('a',1),api._key)
        saved=make_project(result['state'],api._image,result['image_points'])
        self.assertEqual(state['points'],saved['host']['points']);self.assertEqual([],saved['correspondences'])

    def test_clear_review_points_preserves_saved_model_references(self):
        api=Api();state={'session':'new','capture_id':1,'points':[]};api._accept(state)
        review={'document':'saved.ics','points':[{'id':'P1','vertex_id':42}]}
        api._review=review.copy();api._points={'P1':[10,20]}
        with patch.object(api._bridge,'call',return_value=state) as call:
            result=api.clear_points('new',1)
        self.assertTrue(result['ok']);self.assertEqual(1,call.call_count)
        self.assertEqual(review,result['review']);self.assertEqual({},result['image_points'])

    def test_clear_photo_points_restores_preview_without_removing_model_links(self):
        api=Api();state={'session':'a','capture_id':1,'captured':True,'points':[{'id':'P1'}]}
        api._accept(state);api._points={'P1':[10,20]};api._preview_session='a'
        restored={**state,'captured':False,'restored':True}
        with patch.object(api._bridge,'call',side_effect=[state,restored]) as call:
            result=api.clear_points('a',1)
        self.assertTrue(result['ok']);self.assertEqual(('restore','a'),call.call_args.args)
        self.assertEqual(state['points'],result['state']['points']);self.assertEqual({},result['image_points'])

    def test_clear_points_rejects_stale_or_failed_clear_without_losing_points(self):
        state={'session':'a','capture_id':1,'points':[{'id':'P8'}]}
        api=Api();api._accept(state);api._points={'P8':[10,20]}
        with patch.object(api._bridge,'call',return_value=state) as call:
            self.assertFalse(api.clear_points('a',0)['ok']);self.assertEqual(1,call.call_count)
        api._preview_session='a'
        with patch.object(api._bridge,'call',side_effect=[state,RuntimeError('restore failed')]):
            self.assertFalse(api.clear_points('a',1)['ok'])
        self.assertEqual({'P8':[10,20]},api._points)

    def test_scene_save_failure_does_not_write_result(self):
        api=Api();state={'session':'a','capture_id':1,'points':[],'scene_project_supported':True}
        with tempfile.TemporaryDirectory() as directory:
            result_path=Path(directory)/'match.json';scene_path=Path(directory)/'scene.ics'
            state['document']=str(scene_path);api._accept(state)
            api._window=type('Window',(),{'create_file_dialog':lambda *a,**kw:[str(result_path)]})()
            with patch.object(api._bridge,'call',side_effect=[state,RuntimeError('save failed')]):
                result=api.save_project('a',1)
            self.assertFalse(result['ok']);self.assertFalse(result_path.exists())

    def test_save_links_scene_and_open_activates_it(self):
        api=Api();image=load_image(Path(__file__).parent/'fixture/reference.png')
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'match.json';scene=Path(directory)/'scene.ics';scene.write_bytes(b'fixture')
            state={'session':'saved','capture_id':1,'document':str(scene),'points':[{'id':'P1'}],
                   'scene_project_supported':True,'original_camera':{}}
            api._accept(state);api._image=image;api._points={'P1':[10,20]}
            api._window=type('Window',(),{'create_file_dialog':lambda *a,**kw:[str(path)]})()
            with patch.object(api._bridge,'call',return_value=state) as call:
                self.assertTrue(api.save_project('saved',1)['ok'])
                self.assertEqual('save_scene',call.call_args_list[-1].args[0])
            data=json.loads(path.read_text(encoding='utf-8'));self.assertEqual(str(scene),data['scene_path'])
            other={'session':'other','capture_id':0,'document':'other.ics','points':[],'scene_project_supported':True}
            reopened=Api()
            with patch.object(reopened._bridge,'call',side_effect=[other,state]) as call:
                result=reopened._load_project(path)
                self.assertEqual('open_scene',call.call_args_list[-1].args[0])
            self.assertEqual({'P1':[10,20]},result['image_points'])
            scene.unlink()
            with patch.object(reopened._bridge,'call',return_value=other) as call:
                with self.assertRaisesRegex(ValueError,'missing'):reopened._load_project(path)
                self.assertEqual(1,call.call_count)

if __name__=='__main__':unittest.main()

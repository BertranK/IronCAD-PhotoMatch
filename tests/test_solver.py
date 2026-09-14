from pathlib import Path
import sys
import unittest
import json
import numpy as np
from scipy.spatial.transform import Rotation
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'gui'))
from solver import fit_camera


class SolverTests(unittest.TestCase):
    def test_frontal_plane_does_not_claim_a_determined_focal_length(self):
        points = np.array([[0,0,0],[1,0,0],[0,1,0],[1,1,0],[2,0,0],[0,2,0]])
        pixels = points[:, :2]*100+200
        result = fit_camera(points, pixels, 750, 750)
        self.assertFalse(result['stable'])
        self.assertFalse(result['precision_passed'])
        with self.assertRaisesRegex(ValueError, '6개 이상'):
            fit_camera(points[:5], pixels[:5], 750, 750)

    def test_six_planar_points_recover_camera_in_arbitrary_world_plane(self):
        plane = np.array([[-1,-.5,0],[-1,.5,0],[0,-.5,0],[0,.5,0],[1,-.5,0],[1,.5,0]])
        orientation = Rotation.from_euler('xyz', [.4,-.3,.6]).as_matrix()
        camera_rotation = Rotation.from_euler('xyz', [.5,.3,-.2]).as_matrix()
        for scale, offset in [(1,np.zeros(3)),(.001,np.array([1.6,1.9,-1.3]))]:
            world = plane@orientation.T*scale+offset
            q = plane@camera_rotation.T+[.2,-.1,5]
            pixels = q[:,:2]/q[:,2,None]*900+[600,400]
            result = fit_camera(world, pixels, 1200, 800)
            self.assertTrue(result['planar'])
            self.assertTrue(result['stable'])
            self.assertLess(result['max_error_px'],1e-5)
            self.assertAlmostEqual(result['focal_px'],900,places=3)
            np.testing.assert_allclose(result['camera']['direction'],(camera_rotation@orientation.T)[2],atol=1e-5)
            json.dumps(result,allow_nan=False)

    def test_planar_points_require_fixed_optical_center(self):
        points = np.array([[0,0,0],[1,0,0],[0,1,0],[1,1,0],[2,0,0],[0,2,0]])
        with self.assertRaisesRegex(ValueError,'광학 중심 추정'):
            fit_camera(points,points[:,:2]*100+200,750,750,estimate_principal=True)

    def test_six_stud_top_matches_fit_without_changing_model_points(self):
        points = np.array([[-1,0,.5],[-1,0,-.5],[0,0,.5],[0,0,-.5],[1,0,.5],[1,0,-.5]])*.008+[1.6,1.9,-1.3]
        pixels = np.array([[529.2,468.8],[568.9,455.2],[575.6,480.3],[614.6,465.3],[624.3,494.2],[663.7,477.5]])
        original = points.copy()
        result = fit_camera(points,pixels,750,750,initial_camera={'direction':[0,0,1],'up':[0,1,0]})
        self.assertTrue(result['stable']);self.assertTrue(result['planar'])
        self.assertFalse(result['precision_passed'])
        self.assertLess(result['max_error_px'],.7)
        self.assertEqual(6,len(result['point_errors_px']))
        np.testing.assert_array_equal(points,original)

    def test_nearly_planar_initializer_refines_original_depths(self):
        points = np.array([[-1,-.5,0],[-1,.5,0],[0,-.5,0],[0,.5,0],[1,-.5,0],[1,.5,0]])
        points[:,2] = [0,.0001,-.0002,.0001,0,.0002]
        q = points@Rotation.from_euler('xyz',[.5,.3,-.2]).as_matrix().T+[.2,-.1,5]
        pixels = q[:,:2]/q[:,2,None]*900+[600,400]
        result = fit_camera(points,pixels,1200,800)
        self.assertTrue(result['planar']);self.assertTrue(result['stable'])
        self.assertLess(result['max_error_px'],1e-5)

    def test_opencv_free_principal_recovers_cropped_projection_without_calibration_claim(self):
        rng = np.random.default_rng(58)
        points = rng.uniform(-1, 1, (18, 3))
        q = points@Rotation.from_euler('xyz', [.2, -.4, .1]).as_matrix().T + [.6, -.2, 7]
        pixels = q[:, :2]/q[:, 2, None]*850+[692, -4]
        result = fit_camera(points, pixels, 750, 750, estimate_principal=True)
        self.assertLess(result['max_error_px'], 1e-5)
        np.testing.assert_allclose(result['principal_px'], [692, -4], atol=1e-4)
        self.assertFalse(result['precision_passed'])
        self.assertFalse(result['intrinsics_independently_validated'])

    def test_recovers_pose_and_focal_in_original_pixels_at_different_world_scales(self):
        rng = np.random.default_rng(27)
        points = rng.uniform(-1, 1, (12, 3))
        rotation = Rotation.from_euler('xyz', [.3, -.5, .2]).as_matrix()
        camera_points = points@rotation.T + [1, -.4, 8]
        for scale, offset, size in [(1, [0, 0, 0], [1600, 900]), (.001, [10, -30, 15], [900, 1600])]:
            pixels = camera_points[:, :2]/camera_points[:, 2, None]*1800 + np.array(size)/2
            result = fit_camera(points*scale+offset, pixels, *size)
            self.assertLess(result['max_error_px'], 1e-5)
            self.assertAlmostEqual(result['focal_px'], 1800, places=3)
            self.assertTrue(result['precision_passed'])
            self.assertTrue(result['stable'])
            json.dumps(result, allow_nan=False)

    def test_rejects_insufficient_collinear_duplicate_and_nonfinite_points(self):
        rng = np.random.default_rng(5)
        points = rng.uniform(-1, 1, (8, 3)); pixels = rng.uniform(0, 750, (8, 2))
        bad_sets = [points[:5], np.c_[np.arange(8),np.zeros((8,2))], np.full((8, 3), np.nan)]
        duplicate = points.copy(); duplicate[1] = duplicate[0]; bad_sets.append(duplicate)
        for bad in bad_sets:
            with self.assertRaises(ValueError): fit_camera(bad, pixels[:len(bad)], 750, 750)

    def test_noisy_correspondences_are_not_reported_as_precision_pass(self):
        rng = np.random.default_rng(12)
        points = rng.uniform(-1, 1, (14, 3)); q = points+[.2, -.1, 8]
        pixels = q[:, :2]/q[:, 2, None]*1200+375
        pixels += rng.normal(0, 2, pixels.shape)
        result = fit_camera(points, pixels, 750, 750)
        self.assertGreater(result['max_error_px'], .1)
        self.assertFalse(result['precision_passed'])


if __name__ == '__main__': unittest.main()

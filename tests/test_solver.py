from pathlib import Path
import sys
import unittest
import json
import numpy as np
from scipy.spatial.transform import Rotation
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'gui'))
from solver import fit_camera


class SolverTests(unittest.TestCase):
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

    def test_rejects_insufficient_flat_duplicate_and_nonfinite_points(self):
        rng = np.random.default_rng(5)
        points = rng.uniform(-1, 1, (8, 3)); pixels = rng.uniform(0, 750, (8, 2))
        bad_sets = [points[:5], np.c_[points[:, :2], np.zeros(8)], np.full((8, 3), np.nan)]
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

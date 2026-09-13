import sys
from pathlib import Path
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'gui'))
from project import load_image, validate_pixel, make_project


class ProjectTests(unittest.TestCase):
    def test_original_image_dimensions_and_hash(self):
        image = load_image(Path(__file__).parent / 'fixture' / 'reference.png')
        self.assertEqual((1600, 1000), (image['width'], image['height']))
        self.assertEqual(64, len(image['sha256']))
        result = make_project({'points': [{'id': 'P1'}]}, image, {'P1': [400.25, 700.75]})
        self.assertEqual([400.25, 700.75], result['correspondences'][0]['image_px'])
        self.assertNotIn('preview', result['image'])
        self.assertEqual('runtime_matrix_not_completed', result['overall_status'])

    def test_invalid_coordinates_and_stale_ids(self):
        image = {'width': 1600, 'height': 1000}
        for point in [(float('nan'), 0), (0, float('inf')), (-1, 0), (1600, 0), (0, 1000), (True, 3)]:
            with self.assertRaises(ValueError): validate_pixel(*point, image)
        with self.assertRaises(ValueError):
            make_project({'points': [{'id': 'P1'}]}, image, {'P2': [1, 2]})

    def test_bad_image(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'invalid.png';path.write_text('not an image')
            with self.assertRaises(Exception): load_image(path)


if __name__ == '__main__': unittest.main()

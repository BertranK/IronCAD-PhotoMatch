import sys
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image
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

    def test_common_formats_preserve_source_and_create_host_image(self):
        formats = {'PNG': 'png', 'JPEG': 'jpg', 'BMP': 'bmp', 'AVIF': 'avif',
                   'WEBP': 'webp', 'TIFF': 'tiff', 'GIF': 'gif', 'ICO': 'ico',
                   'JPEG2000': 'jp2', 'PPM': 'ppm', 'TGA': 'tga', 'PCX': 'pcx',
                   'DDS': 'dds', 'QOI': 'qoi'}
        with tempfile.TemporaryDirectory() as directory, patch('paths.RUNTIME', Path(directory)/'.runtime'):
            for format_name, extension in formats.items():
                with self.subTest(format=format_name):
                    path = Path(directory)/('sample.'+extension)
                    Image.new('RGB', (32, 32), (210, 70, 20)).save(path, format=format_name)
                    original = path.read_bytes()
                    result = load_image(path)
                    self.assertEqual((32, 32), (result['width'], result['height']))
                    self.assertEqual(original, path.read_bytes())
                    self.assertEqual(str(path.resolve()), result['path'])
                    with Image.open(result['overlay_path']) as overlay:
                        self.assertIn(overlay.format, {'PNG', 'JPEG', 'BMP'})
                        self.assertEqual((32, 32), overlay.size)
                        overlay.load()

    def test_animated_and_multipage_images_use_first_frame(self):
        with tempfile.TemporaryDirectory() as directory, patch('paths.RUNTIME', Path(directory)/'.runtime'):
            for extension in ('gif', 'tiff', 'webp', 'png'):
                with self.subTest(format=extension):
                    path = Path(directory)/('frames.'+extension)
                    Image.new('RGB', (32, 32), 'red').save(path, save_all=True,
                        append_images=[Image.new('RGB', (32, 32), 'blue')], lossless=True)
                    result = load_image(path)
                    with Image.open(result['overlay_path']) as overlay:
                        self.assertEqual('PNG', overlay.format)
                        self.assertEqual(1, getattr(overlay, 'n_frames', 1))
                        self.assertEqual((255, 0, 0), overlay.convert('RGB').getpixel((0, 0)))

    def test_vector_format_is_rejected_before_rendering(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'drawing.eps'
            Image.new('RGB', (8, 8)).save(path)
            with self.assertRaises(ValueError): load_image(path)


if __name__ == '__main__': unittest.main()

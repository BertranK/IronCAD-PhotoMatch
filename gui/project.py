"""Image coordinates stay in original, unrotated source pixels."""
import base64
import hashlib
import io
import math
from pathlib import Path
from PIL import Image

SUPPORTED_IMAGE_FORMATS = {'PNG', 'JPEG', 'BMP', 'AVIF', 'WEBP', 'TIFF', 'GIF',
                           'ICO', 'JPEG2000', 'PPM', 'TGA', 'PCX', 'DDS', 'QOI'}
IMAGE_FILE_PATTERNS = ';'.join('*'+extension for extension, format_name in
                             sorted(Image.registered_extensions().items())
                             if format_name in SUPPORTED_IMAGE_FORMATS)


def load_image(path):
    path = Path(path).resolve(strict=True)
    if path.stat().st_size > 100*1024*1024:
        raise ValueError("100 MB 이하의 사진을 선택하세요.")
    with Image.open(path) as image:
        if image.format not in SUPPORTED_IMAGE_FORMATS:
            raise ValueError("지원하는 이미지 형식의 파일을 선택하세요.")
        width, height = image.size
        if width*height > 40_000_000:
            raise ValueError("4천만 픽셀 이하의 사진을 선택하세요.")
        image.load()
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        overlay_path = path
        if image.format not in {'PNG', 'JPEG', 'BMP'} or getattr(image, 'is_animated', False):
            from paths import RUNTIME
            cache = RUNTIME / 'images'
            cache.mkdir(parents=True, exist_ok=True)
            overlay_path = cache / (digest + '.png')
            if not overlay_path.exists(): image.convert('RGBA').save(overlay_path)
        preview = image.convert("RGB")
        preview.thumbnail((2400, 2400))
        output = io.BytesIO()
        preview.save(output, format="JPEG", quality=92)
    return {"path": str(path), "name": path.name, "width": width, "height": height,
            "sha256": digest, "overlay_path": str(overlay_path),
            "preview": "data:image/jpeg;base64,"+base64.b64encode(output.getvalue()).decode("ascii")}


def validate_pixel(x, y, image):
    if isinstance(x, bool) or isinstance(y, bool):
        raise ValueError("올바른 사진 위치를 선택하세요.")
    x, y = float(x), float(y)
    if not (math.isfinite(x) and math.isfinite(y) and 0 <= x < image["width"] and 0 <= y < image["height"]):
        raise ValueError("사진 안의 점을 선택하세요.")
    return [x, y]


def make_project(state, image, points):
    ids = {p["id"] for p in state["points"]}
    if not set(points) <= ids:
        raise ValueError("모델의 대응점이 바뀌었습니다.")
    return {"schema_version": 2, "overall_status": "runtime_matrix_not_completed",
            "image": {k: v for k, v in (image or {}).items() if k not in {"preview", "overlay_path"}},
            "image_coordinate_system": "original_pixels_top_left_unrotated",
            "correspondences": [{"id": key, "image_px": validate_pixel(*pixel, image)} for key, pixel in points.items()],
            "host": state}

"""
AgroMate Image Processing Utilities

Helper functions for image preprocessing, resizing, format conversion,
and preparation for AI model inference.
"""

import io
import base64
import logging
from pathlib import Path
from typing import Optional, Tuple

import numpy as np
from PIL import Image, ImageEnhance, ImageFilter

logger = logging.getLogger(__name__)

# Standard input sizes for AI models
MODEL_INPUT_SIZE = (224, 224)  # Standard for MobileNet, EfficientNet, etc.
DISPLAY_THUMBNAIL_SIZE = (128, 128)
MAX_IMAGE_SIZE = 10 * 1024 * 1024  # 10 MB


def load_image(
    source: bytes | str | Path,
    target_size: Tuple[int, int] = None,
) -> Optional[Image.Image]:
    """
    Load an image from bytes, file path, or base64 string.

    Args:
        source: Image data as bytes, file path, or base64 string
        target_size: Optional (width, height) to resize to

    Returns:
        PIL Image or None on failure
    """
    try:
        if isinstance(source, (str, Path)):
            path = Path(source)
            if path.exists():
                img = Image.open(path)
            else:
                # Try base64 decode
                img_data = base64.b64decode(source)
                img = Image.open(io.BytesIO(img_data))
        elif isinstance(source, bytes):
            img = Image.open(io.BytesIO(source))
        else:
            logger.error(f"Unsupported image source type: {type(source)}")
            return None

        # Convert to RGB if needed
        if img.mode != "RGB":
            img = img.convert("RGB")

        if target_size:
            img = img.resize(target_size, Image.Resampling.LANCZOS)

        return img

    except Exception as e:
        logger.error(f"Failed to load image: {e}")
        return None


def preprocess_for_model(
    image: Image.Image,
    input_size: Tuple[int, int] = MODEL_INPUT_SIZE,
    normalize: bool = True,
) -> Optional[np.ndarray]:
    """
    Preprocess an image for AI model inference.

    Args:
        image: PIL Image
        input_size: Model input dimensions (width, height)
        normalize: Whether to normalize pixel values to [0, 1]

    Returns:
        Numpy array ready for model input (1, H, W, 3)
    """
    try:
        # Resize
        img = image.resize(input_size, Image.Resampling.LANCZOS)

        # Convert to numpy array
        arr = np.array(img, dtype=np.float32)

        if normalize:
            arr = arr / 255.0

        # Add batch dimension
        arr = np.expand_dims(arr, axis=0)

        return arr

    except Exception as e:
        logger.error(f"Image preprocessing failed: {e}")
        return None


def enhance_plant_image(image: Image.Image) -> Image.Image:
    """
    Enhance a plant image for better AI recognition.

    Applies subtle sharpening and contrast enhancement
    optimized for leaf/plant photography.
    """
    # Slight sharpening to bring out leaf details
    img = image.filter(ImageFilter.SHARPEN)

    # Boost contrast slightly
    enhancer = ImageEnhance.Contrast(img)
    img = enhancer.enhance(1.15)

    # Boost color saturation slightly
    enhancer = ImageEnhance.Color(img)
    img = enhancer.enhance(1.1)

    return img


def image_to_bytes(
    image: Image.Image,
    format: str = "JPEG",
    quality: int = 85,
) -> bytes:
    """Convert a PIL Image to bytes."""
    buffer = io.BytesIO()
    image.save(buffer, format=format, quality=quality)
    return buffer.getvalue()


def image_to_base64(
    image: Image.Image,
    format: str = "JPEG",
    quality: int = 85,
) -> str:
    """Convert a PIL Image to base64 string."""
    img_bytes = image_to_bytes(image, format, quality)
    return base64.b64encode(img_bytes).decode("utf-8")


def create_thumbnail(
    image: Image.Image,
    size: Tuple[int, int] = DISPLAY_THUMBNAIL_SIZE,
) -> Image.Image:
    """Create a thumbnail for display purposes."""
    img = image.copy()
    img.thumbnail(size, Image.Resampling.LANCZOS)
    return img


def validate_image(data: bytes) -> Tuple[bool, str]:
    """
    Validate image data for processing.

    Returns:
        Tuple of (is_valid, error_message)
    """
    if not data:
        return False, "Empty image data"

    if len(data) > MAX_IMAGE_SIZE:
        return False, f"Image too large: {len(data)} bytes (max {MAX_IMAGE_SIZE})"

    try:
        img = Image.open(io.BytesIO(data))
        img.verify()
        return True, "OK"
    except Exception as e:
        return False, f"Invalid image: {e}"


def crop_center(image: Image.Image, target_ratio: float = 1.0) -> Image.Image:
    """Crop image to center with target aspect ratio."""
    w, h = image.size
    current_ratio = w / h

    if current_ratio > target_ratio:
        # Wider than target, crop width
        new_w = int(h * target_ratio)
        left = (w - new_w) // 2
        return image.crop((left, 0, left + new_w, h))
    else:
        # Taller than target, crop height
        new_h = int(w / target_ratio)
        top = (h - new_h) // 2
        return image.crop((0, top, w, top + new_h))


def convert_raw_to_jpeg(data: bytes) -> bytes:
    """
    Check if the input bytes match raw camera formats (Grayscale or YUV422) at 320x240
    and convert them to standard JPEG bytes using OpenCV for superior quality.
    If it's already a JPEG, return it as-is.
    """
    import cv2
    import numpy as np

    # QVGA Grayscale = 320 * 240 * 1 = 76800 bytes
    if len(data) == 76800:
        try:
            logger.info("Converting raw 320x240 Grayscale to JPEG using OpenCV...")
            gray_img = np.frombuffer(data, dtype=np.uint8).reshape((240, 320))
            success, jpeg_data = cv2.imencode('.jpg', gray_img, [cv2.IMWRITE_JPEG_QUALITY, 95])
            if success:
                return bytes(jpeg_data)
        except Exception as e:
            logger.error(f"Failed to convert raw Grayscale to JPEG: {e}")
            
    # QVGA YUV422 (YUYV) = 320 * 240 * 2 = 153600 bytes
    elif len(data) == 153600:
        try:
            logger.info("Converting raw 320x240 YUV422 to JPEG using OpenCV...")
            arr = np.frombuffer(data, dtype=np.uint8).reshape((240, 320, 2))
            
            # Use OpenCV's native YUV422 (YUYV) to BGR conversion — correct for OV7670
            bgr_img = cv2.cvtColor(arr, cv2.COLOR_YUV2BGR_YUYV)
            
            # --- Image enhancement pipeline for vivid plant photography ---
            
            # 1. Apply CLAHE (Contrast Limited Adaptive Histogram Equalization)
            #    for adaptive contrast boost that matches the sensor's contrast=2 setting
            lab = cv2.cvtColor(bgr_img, cv2.COLOR_BGR2LAB)
            l_ch, a_ch, b_ch = cv2.split(lab)
            clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(4, 4))
            l_ch = clahe.apply(l_ch)
            lab = cv2.merge([l_ch, a_ch, b_ch])
            bgr_img = cv2.cvtColor(lab, cv2.COLOR_LAB2BGR)
            
            # 2. Boost color saturation to match sensor's saturation=2 setting
            #    (vivid greens for leaf photography)
            hsv = cv2.cvtColor(bgr_img, cv2.COLOR_BGR2HSV).astype(np.float32)
            hsv[:, :, 1] = np.clip(hsv[:, :, 1] * 1.4, 0, 255)  # 40% saturation boost
            bgr_img = cv2.cvtColor(hsv.astype(np.uint8), cv2.COLOR_HSV2BGR)
            
            # 3. Apply unsharp mask for crisp leaf detail
            gaussian = cv2.GaussianBlur(bgr_img, (0, 0), 2.0)
            bgr_img = cv2.addWeighted(bgr_img, 1.5, gaussian, -0.5, 0)
            
            success, jpeg_data = cv2.imencode('.jpg', bgr_img, [cv2.IMWRITE_JPEG_QUALITY, 95])
            if success:
                return bytes(jpeg_data)
        except Exception as e:
            logger.error(f"Failed to convert raw YUV422 to JPEG: {e}")

    return data



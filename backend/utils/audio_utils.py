"""
AgroMate Audio Processing Utilities

Helper functions for audio format conversion, resampling,
compression, and preparation for voice processing.
"""

import io
import struct
import logging
from typing import Optional, Tuple

import numpy as np

logger = logging.getLogger(__name__)

# Audio constants
SAMPLE_RATE = 16000  # 16kHz standard for speech
CHANNELS = 1  # Mono
SAMPLE_WIDTH = 2  # 16-bit
MAX_AUDIO_DURATION = 30  # Maximum recording duration in seconds
MAX_AUDIO_SIZE = SAMPLE_RATE * SAMPLE_WIDTH * MAX_AUDIO_DURATION  # ~960KB


def pcm_to_wav(pcm_data: bytes, sample_rate: int = SAMPLE_RATE, channels: int = CHANNELS, sample_width: int = SAMPLE_WIDTH) -> bytes:
    """
    Convert raw PCM audio data to WAV format.

    Args:
        pcm_data: Raw PCM audio bytes
        sample_rate: Sample rate in Hz
        channels: Number of audio channels
        sample_width: Bytes per sample (2 for 16-bit)

    Returns:
        WAV file bytes
    """
    data_size = len(pcm_data)
    file_size = 36 + data_size

    wav = io.BytesIO()

    # RIFF header
    wav.write(b"RIFF")
    wav.write(struct.pack("<I", file_size))
    wav.write(b"WAVE")

    # Format chunk
    wav.write(b"fmt ")
    wav.write(struct.pack("<I", 16))  # Chunk size
    wav.write(struct.pack("<H", 1))  # PCM format
    wav.write(struct.pack("<H", channels))
    wav.write(struct.pack("<I", sample_rate))
    wav.write(struct.pack("<I", sample_rate * channels * sample_width))  # Byte rate
    wav.write(struct.pack("<H", channels * sample_width))  # Block align
    wav.write(struct.pack("<H", sample_width * 8))  # Bits per sample

    # Data chunk
    wav.write(b"data")
    wav.write(struct.pack("<I", data_size))
    wav.write(pcm_data)

    return wav.getvalue()


def wav_to_pcm(wav_data: bytes) -> Tuple[bytes, int, int, int]:
    """
    Extract raw PCM data from WAV format.

    Returns:
        Tuple of (pcm_data, sample_rate, channels, sample_width)
    """
    buf = io.BytesIO(wav_data)

    # Read RIFF header
    riff = buf.read(4)
    if riff != b"RIFF":
        raise ValueError("Not a valid WAV file (missing RIFF header)")

    buf.read(4)  # File size
    wave = buf.read(4)
    if wave != b"WAVE":
        raise ValueError("Not a valid WAV file (missing WAVE marker)")

    # Read chunks
    sample_rate = SAMPLE_RATE
    channels = CHANNELS
    sample_width = SAMPLE_WIDTH
    pcm_data = b""

    while True:
        chunk_id = buf.read(4)
        if len(chunk_id) < 4:
            break

        chunk_size = struct.unpack("<I", buf.read(4))[0]

        if chunk_id == b"fmt ":
            fmt_data = buf.read(chunk_size)
            _, channels, sample_rate, _, _, bits_per_sample = struct.unpack(
                "<HHIIHH", fmt_data[:16]
            )
            sample_width = bits_per_sample // 8

        elif chunk_id == b"data":
            pcm_data = buf.read(chunk_size)

        else:
            buf.read(chunk_size)

    return pcm_data, sample_rate, channels, sample_width


def resample_audio(
    pcm_data: bytes,
    original_rate: int,
    target_rate: int = SAMPLE_RATE,
    sample_width: int = SAMPLE_WIDTH,
) -> bytes:
    """
    Resample audio to target sample rate using linear interpolation.

    Args:
        pcm_data: Raw PCM audio data
        original_rate: Original sample rate
        target_rate: Target sample rate
        sample_width: Bytes per sample

    Returns:
        Resampled PCM data
    """
    if original_rate == target_rate:
        return pcm_data

    # Convert to numpy
    dtype = np.int16 if sample_width == 2 else np.int8
    samples = np.frombuffer(pcm_data, dtype=dtype).astype(np.float32)

    # Calculate ratio
    ratio = target_rate / original_rate
    new_length = int(len(samples) * ratio)

    # Linear interpolation
    indices = np.linspace(0, len(samples) - 1, new_length)
    resampled = np.interp(indices, np.arange(len(samples)), samples)

    return resampled.astype(dtype).tobytes()


def normalize_audio(pcm_data: bytes, sample_width: int = SAMPLE_WIDTH) -> bytes:
    """
    Normalize audio volume to use full dynamic range.
    """
    dtype = np.int16 if sample_width == 2 else np.int8
    samples = np.frombuffer(pcm_data, dtype=dtype).astype(np.float32)

    max_val = np.abs(samples).max()
    if max_val < 1:
        return pcm_data

    max_range = 32767 if sample_width == 2 else 127
    scale = max_range / max_val * 0.95  # Leave 5% headroom

    normalized = (samples * scale).clip(-max_range, max_range)
    return normalized.astype(dtype).tobytes()


def calculate_energy(pcm_data: bytes, sample_width: int = SAMPLE_WIDTH) -> float:
    """
    Calculate RMS energy of audio data.

    Returns a float representing audio energy level (0.0 = silence).
    """
    dtype = np.int16 if sample_width == 2 else np.int8
    samples = np.frombuffer(pcm_data, dtype=dtype).astype(np.float32)

    if len(samples) == 0:
        return 0.0

    rms = np.sqrt(np.mean(samples ** 2))
    max_range = 32767 if sample_width == 2 else 127
    return float(rms / max_range)


def detect_voice_activity(
    pcm_data: bytes,
    sample_rate: int = SAMPLE_RATE,
    frame_ms: int = 30,
    energy_threshold: float = 0.02,
    sample_width: int = SAMPLE_WIDTH,
) -> list[Tuple[float, float]]:
    """
    Simple energy-based Voice Activity Detection (VAD).

    Returns list of (start_time, end_time) tuples where speech is detected.
    """
    dtype = np.int16 if sample_width == 2 else np.int8
    samples = np.frombuffer(pcm_data, dtype=dtype).astype(np.float32)
    max_range = 32767.0 if sample_width == 2 else 127.0
    samples = samples / max_range

    frame_size = int(sample_rate * frame_ms / 1000)
    num_frames = len(samples) // frame_size

    segments = []
    in_speech = False
    start_time = 0.0

    for i in range(num_frames):
        frame = samples[i * frame_size : (i + 1) * frame_size]
        energy = np.sqrt(np.mean(frame ** 2))
        current_time = i * frame_ms / 1000.0

        if energy > energy_threshold and not in_speech:
            in_speech = True
            start_time = current_time
        elif energy <= energy_threshold and in_speech:
            in_speech = False
            segments.append((start_time, current_time))

    if in_speech:
        segments.append((start_time, num_frames * frame_ms / 1000.0))

    return segments


def trim_silence(
    pcm_data: bytes,
    sample_rate: int = SAMPLE_RATE,
    threshold: float = 0.01,
    sample_width: int = SAMPLE_WIDTH,
) -> bytes:
    """
    Trim silence from the beginning and end of audio.
    """
    segments = detect_voice_activity(
        pcm_data, sample_rate, energy_threshold=threshold, sample_width=sample_width
    )

    if not segments:
        return pcm_data

    start_sample = int(segments[0][0] * sample_rate) * sample_width
    end_sample = int(segments[-1][1] * sample_rate) * sample_width

    # Add small padding
    padding = int(0.1 * sample_rate) * sample_width
    start_sample = max(0, start_sample - padding)
    end_sample = min(len(pcm_data), end_sample + padding)

    return pcm_data[start_sample:end_sample]


def compress_for_ble(
    pcm_data: bytes,
    sample_width: int = SAMPLE_WIDTH,
) -> bytes:
    """
    Compress audio for BLE transfer using µ-law encoding.

    Reduces 16-bit audio to 8-bit while preserving dynamic range.
    """
    if sample_width != 2:
        return pcm_data

    samples = np.frombuffer(pcm_data, dtype=np.int16).astype(np.float32)

    # µ-law compression
    mu = 255.0
    max_val = 32768.0
    normalized = samples / max_val
    compressed = np.sign(normalized) * np.log1p(mu * np.abs(normalized)) / np.log1p(mu)

    # Scale to 8-bit
    encoded = ((compressed + 1.0) * 127.5).clip(0, 255).astype(np.uint8)
    return encoded.tobytes()


def decompress_from_ble(compressed_data: bytes) -> bytes:
    """
    Decompress µ-law encoded audio back to 16-bit PCM.
    """
    encoded = np.frombuffer(compressed_data, dtype=np.uint8).astype(np.float32)

    # Reverse µ-law
    mu = 255.0
    normalized = encoded / 127.5 - 1.0
    expanded = np.sign(normalized) * (1.0 / mu) * ((1.0 + mu) ** np.abs(normalized) - 1.0)

    # Scale to 16-bit
    samples = (expanded * 32768.0).clip(-32768, 32767).astype(np.int16)
    return samples.tobytes()


def validate_audio(data: bytes) -> Tuple[bool, str]:
    """
    Validate audio data.

    Returns:
        Tuple of (is_valid, error_message)
    """
    if not data:
        return False, "Empty audio data"

    if len(data) > MAX_AUDIO_SIZE:
        return False, f"Audio too large: {len(data)} bytes"

    if len(data) < 100:
        return False, "Audio too short"

    return True, "OK"

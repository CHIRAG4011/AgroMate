"""
Generate spoken PCM audio clips for ESP32 firmware embedding.
Outputs C header files with uint8_t arrays of 16-bit PCM @ 16kHz mono.
"""
import asyncio
import struct
import sys
import os

# Add project root to path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

async def generate_clip(text: str, name: str, output_dir: str):
    """Generate a spoken audio clip and save as C header array."""
    import edge_tts
    import io
    
    # Generate MP3 with edge-tts
    communicate = edge_tts.Communicate(text, voice="en-US-AriaNeural", rate="+10%")
    mp3_data = b""
    async for chunk in communicate.stream():
        if chunk["type"] == "audio":
            mp3_data += chunk["data"]
    
    if not mp3_data:
        print(f"  ERROR: No audio generated for '{text}'")
        return
    
    print(f"  MP3 size: {len(mp3_data)} bytes")
    
    # Convert MP3 to 16kHz 16-bit mono PCM
    try:
        import soundfile as sf
        import numpy as np
        
        mp3_io = io.BytesIO(mp3_data)
        audio_data, sr = sf.read(mp3_io, dtype='int16')
        
        # Convert stereo to mono if needed
        if len(audio_data.shape) > 1:
            audio_data = audio_data.mean(axis=1).astype(np.int16)
        
        # Resample to 16kHz if needed
        if sr != 16000:
            from scipy.signal import resample
            num_samples = int(len(audio_data) * 16000 / sr)
            audio_data = resample(audio_data.astype(np.float64), num_samples).astype(np.int16)
        
        # Clean normalization to 85% max (prevents square-wave digital clipping buzzing)
        max_val = np.max(np.abs(audio_data))
        if max_val > 0:
            scale = 28000.0 / max_val
            audio_data = (audio_data.astype(np.float64) * scale).clip(-32000, 32000).astype(np.int16)
        
        pcm_bytes = audio_data.tobytes()
        
    except Exception as e:
        print(f"  Soundfile failed: {e}, trying pydub...")
        from pydub import AudioSegment
        import numpy as np
        
        mp3_io = io.BytesIO(mp3_data)
        audio = AudioSegment.from_mp3(mp3_io)
        audio = audio.set_frame_rate(16000).set_channels(1).set_sample_width(2)
        
        # Normalize volume
        audio = audio.normalize()
        audio = audio + 6  # +6dB boost
        
        pcm_bytes = audio.raw_data
    
    print(f"  PCM size: {len(pcm_bytes)} bytes ({len(pcm_bytes)/32000:.1f}s)")
    
    # Write C header file
    header_path = os.path.join(output_dir, f"voice_{name}.h")
    with open(header_path, "w") as f:
        f.write(f"// Auto-generated voice clip: \"{text}\"\n")
        f.write(f"// 16kHz, 16-bit mono PCM, {len(pcm_bytes)} bytes ({len(pcm_bytes)/32000:.1f}s)\n")
        f.write(f"#pragma once\n")
        f.write(f"#include <Arduino.h>\n\n")
        f.write(f"const size_t VOICE_{name.upper()}_SIZE = {len(pcm_bytes)};\n")
        f.write(f"const uint8_t VOICE_{name.upper()}[] PROGMEM = {{\n")
        
        # Write bytes in rows of 16
        for i in range(0, len(pcm_bytes), 16):
            chunk = pcm_bytes[i:i+16]
            hex_vals = ", ".join(f"0x{b:02X}" for b in chunk)
            if i + 16 >= len(pcm_bytes):
                f.write(f"    {hex_vals}\n")
            else:
                f.write(f"    {hex_vals},\n")
        
        f.write(f"}};\n")
    
    print(f"  Saved: {header_path}")
    return len(pcm_bytes)


async def main():
    output_dir = r"c:\Users\chira\OneDrive\Desktop\AgroMate\firmware\src\audio\voices"
    os.makedirs(output_dir, exist_ok=True)
    
    clips = [
        ("Scanning started. Please hold the camera steady near the plant.", "scan_start"),
        ("Scan completed. Analyzing your plant now.", "scan_complete"),
    ]
    
    total_flash = 0
    for text, name in clips:
        print(f"\nGenerating: {name} -> \"{text}\"")
        size = await generate_clip(text, name, output_dir)
        if size:
            total_flash += size
    
    print(f"\n{'='*50}")
    print(f"Total flash usage: {total_flash:,} bytes ({total_flash/1024:.1f} KB)")
    print(f"ESP32-S3 flash: 8MB — this uses {total_flash/8388608*100:.2f}%")

if __name__ == "__main__":
    asyncio.run(main())

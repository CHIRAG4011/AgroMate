import soundfile as sf
import numpy as np
import scipy.signal
import os

mp3_path = "Voices/ElevenLabs_2026-09-19T05_03_50_Eva - Futuristic Robot Helper_pvc_sp100_s42_sb78_se77_b_m2.mp3"
data, sr = sf.read(mp3_path)
print(f"Original: {data.shape} @ {sr} Hz ({len(data)/sr:.2f}s)")

if len(data.shape) > 1:
    data = data.mean(axis=1)

# Remove any slight DC offset
data = data - np.mean(data)

# High-fidelity polyphase resampling to 16,000 Hz with Kaiser window (beta=9.0)
# This provides >85dB stopband rejection with pristine anti-aliasing
gcd = np.gcd(16000, sr)
up = 16000 // gcd
down = sr // gcd
resampled = scipy.signal.resample_poly(data, up, down, window=("kaiser", 9.0))
print(f"Resampled: {len(resampled)} samples ({len(resampled)/16000:.2f}s)")

# 120 Hz 2nd-order Butterworth high-pass filter to remove subsonic rumble that distorts small speakers
sos = scipy.signal.butter(2, 120, 'highpass', fs=16000, output='sos')
resampled = scipy.signal.sosfilt(sos, resampled)

# Peak normalize to 70% full scale (-3 dBFS) to guarantee zero DAC/amplifier clipping
peak = np.max(np.abs(resampled))
resampled = (resampled / peak) * 0.70
pcm16 = (resampled * 32767).astype(np.int16)

raw_bytes = pcm16.tobytes()
total_bytes = len(raw_bytes)
print(f"PCM size: {total_bytes} bytes")

out_header = "firmware/src/audio/voices/voice_boot.h"
with open(out_header, "w") as f:
    f.write("// Auto-generated voice clip: Eva - Futuristic Robot Helper\n")
    f.write(f"// 16kHz, 16-bit mono PCM, {total_bytes} bytes ({len(pcm16)/16000:.2f}s)\n")
    f.write("#pragma once\n")
    f.write("#include <Arduino.h>\n\n")
    f.write(f"const size_t VOICE_BOOT_SIZE = {total_bytes};\n")
    f.write("const uint8_t VOICE_BOOT[] PROGMEM = {\n")
    
    CHUNK = 16
    lines = []
    for i in range(0, total_bytes, CHUNK):
        chunk = raw_bytes[i:i+CHUNK]
        hex_strs = [f"0x{b:02X}" for b in chunk]
        line = "    " + ", ".join(hex_strs)
        if i + CHUNK < total_bytes:
            line += ",\n"
        else:
            line += "\n"
        lines.append(line)
        if len(lines) >= 1000:
            f.writelines(lines)
            lines = []
            
    if lines:
        f.writelines(lines)
    f.write("};\n")

print(f"Written successfully to {out_header}! File size: {os.path.getsize(out_header)} bytes")

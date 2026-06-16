import wave
import math
import struct
import os

SAMPLE_RATE = 44100
MAX_AMP = 32767

def generate_wav(filename, duration, generate_sample_func):
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    num_samples = int(SAMPLE_RATE * duration)
    
    with wave.open(filename, 'w') as wav_file:
        wav_file.setnchannels(1)
        wav_file.setsampwidth(2)
        wav_file.setframerate(SAMPLE_RATE)
        
        for i in range(num_samples):
            t = float(i) / SAMPLE_RATE
            sample = generate_sample_func(t, num_samples, i)
            # Clip
            sample = max(-1.0, min(1.0, sample))
            packed_value = struct.pack('h', int(sample * MAX_AMP))
            wav_file.writeframesraw(packed_value)

# 1. send.wav - Soft, reassuring chime (Sine wave with quick attack, long release)
def send_sound(t, total_samples, i):
    freq = 880.0 # A5
    # Envelope
    attack_time = 0.01
    if t < attack_time:
        env = t / attack_time
    else:
        env = math.exp(-(t - attack_time) * 4.0) # Decay
    
    val = math.sin(2.0 * math.pi * freq * t) * env
    # Add a slight harmonic
    val += 0.3 * math.sin(2.0 * math.pi * (freq * 2) * t) * env
    return val * 0.4

# 2. zen_mode.wav - Atmospheric swish/ambient (Low frequency sweep with slow attack/release)
def zen_sound(t, total_samples, i):
    duration = total_samples / SAMPLE_RATE
    # Envelope (fade in / out)
    if t < 0.5:
        env = t / 0.5
    elif t > duration - 0.5:
        env = (duration - t) / 0.5
    else:
        env = 1.0
    
    freq = 150.0 + 50.0 * math.sin(2.0 * math.pi * 0.2 * t)
    val = math.sin(2.0 * math.pi * freq * t) * env
    
    # Add noise / airiness
    import random
    val += 0.1 * random.uniform(-1, 1) * env
    
    return val * 0.3

# 3. focus_finish.wav - Soft bell chord (Multiple sine waves)
def focus_finish_sound(t, total_samples, i):
    # C major chord: C5 (523.25), E5 (659.25), G5 (783.99)
    freqs = [523.25, 659.25, 783.99, 1046.50]
    
    attack_time = 0.05
    if t < attack_time:
        env = t / attack_time
    else:
        env = math.exp(-(t - attack_time) * 2.0)
        
    val = 0
    for f in freqs:
        val += math.sin(2.0 * math.pi * f * t) * env
    
    return (val / len(freqs)) * 0.5

# 4. hit.wav - Very short pop/glass clink for physics
def hit_sound(t, total_samples, i):
    # Quick frequency drop (pew/pop effect)
    freq = 1200.0 * math.exp(-t * 30.0)
    
    env = math.exp(-t * 20.0)
    val = math.sin(2.0 * math.pi * freq * t) * env
    return val * 0.15 # lower volume so it doesn't get annoying

if __name__ == "__main__":
    generate_wav("Assets/Sounds/send.wav", 1.5, send_sound)
    generate_wav("Assets/Sounds/zen_mode.wav", 2.0, zen_sound)
    generate_wav("Assets/Sounds/focus_finish.wav", 3.0, focus_finish_sound)
    generate_wav("Assets/Sounds/hit.wav", 0.2, hit_sound)
    print("Sounds generated successfully in Assets/Sounds/")

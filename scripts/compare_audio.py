#!/usr/bin/env python3
"""Compare PCM WAV captures without hiding level mismatch. Requires NumPy."""
import argparse
import json
import wave
from pathlib import Path
import numpy as np


def read(path):
    with wave.open(str(path), "rb") as wav:
        rate, channels, width = wav.getframerate(), wav.getnchannels(), wav.getsampwidth()
        raw = wav.readframes(wav.getnframes())
    if width == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        x = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        x = ((x ^ 0x800000) - 0x800000) / 8388608.0
    elif width in (2, 4):
        x = np.frombuffer(raw, dtype=f"<i{width}").astype(float) / (2 ** (8 * width - 1))
    else:
        raise ValueError("Only 16/24/32-bit PCM WAV is supported")
    return rate, x.reshape(-1, channels)


def compare(reference, candidate, max_lag=2048):
    if reference.shape != candidate.shape:
        raise ValueError("Capture frame/channel counts differ")
    if not np.any(reference) or not np.any(candidate):
        raise ValueError("Silent reference or candidate: cannot measure parity")
    n = reference.shape[0]
    fft_size = 1 << (2 * n - 1).bit_length()
    # Correlate first channel only for delay estimation; measure both channels.
    correlation = np.fft.irfft(np.fft.rfft(candidate[:, 0], fft_size) *
                              np.conj(np.fft.rfft(reference[:, 0], fft_size)), fft_size)
    lags = np.arange(-min(max_lag, n - 1), min(max_lag, n - 1) + 1)
    lag = int(lags[np.argmax(np.abs(correlation[lags % fft_size]))])
    if lag >= 0:
        a, b = reference[:n-lag], candidate[lag:]
    else:
        a, b = reference[-lag:], candidate[:n+lag]
    error = b - a
    energy = float(np.sum(a*a))
    gain = float(np.sum(a*b) / energy)
    relative_error = float(np.sqrt(np.sum(error*error) / energy))
    return {"candidate_delay_frames": lag,
            "raw_peak_error": float(np.max(np.abs(candidate-reference))),
            "aligned_peak_error": float(np.max(np.abs(error))),
            "aligned_relative_rms_error": relative_error,
            "candidate_gain_relative_to_reference": gain,
            "gain_corrected_relative_rms_error": float(np.sqrt(np.sum((b-gain*a)**2)/energy))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    args = parser.parse_args()
    rate, reference = read(args.reference)
    other_rate, candidate = read(args.candidate)
    if rate != other_rate:
        raise ValueError("Sample rates differ; do not resample reference measurements")
    print(json.dumps({"sample_rate": rate, **compare(reference, candidate)}, indent=2))


if __name__ == "__main__":
    main()

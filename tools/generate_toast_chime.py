#!/usr/bin/env python3
"""Generate the original, small confirmation chime packaged with the mod."""

from __future__ import annotations

import argparse
import math
import struct
import wave
from pathlib import Path


SAMPLE_RATE = 48_000
DURATION_SECONDS = 0.44
PCM_PEAK = 0.31


def bell_envelope(time: float, start: float, length: float) -> float:
    """A short raised-cosine attack followed by a smooth decay."""
    relative = time - start
    if relative < 0.0 or relative >= length:
        return 0.0
    attack = 0.012
    if relative < attack:
        return 0.5 - 0.5 * math.cos(math.pi * relative / attack)
    progress = (relative - attack) / (length - attack)
    return (1.0 - progress) ** 1.7


def bell_tone(time: float, start: float, length: float, frequency: float, gain: float) -> float:
    """A mellow sine tone with a quiet upper partial for a bell-like colour."""
    envelope = bell_envelope(time, start, length)
    phase = 2.0 * math.pi * frequency * (time - start)
    return envelope * gain * (math.sin(phase) + 0.22 * math.sin(2.0 * phase))


def samples() -> list[int]:
    count = round(SAMPLE_RATE * DURATION_SECONDS)
    pcm: list[int] = []
    for index in range(count):
        time = index / SAMPLE_RATE
        value = (
            bell_tone(time, 0.0, 0.29, 523.25, 0.135)
            + bell_tone(time, 0.115, DURATION_SECONDS - 0.115, 659.25, 0.115)
        )
        value = max(-PCM_PEAK, min(PCM_PEAK, value))
        pcm.append(round(value * 32767.0))
    return pcm


def write_chime(destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    encoded = struct.pack("<{}h".format(round(SAMPLE_RATE * DURATION_SECONDS)), *samples())
    with wave.open(str(destination), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(encoded)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="WAV file to create")
    args = parser.parse_args()
    write_chime(args.destination)


if __name__ == "__main__":
    main()

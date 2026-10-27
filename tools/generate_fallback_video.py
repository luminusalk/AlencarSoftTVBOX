"""Generate the small, offline MPEG-TS background used when live TV fails."""

from math import sin, tau
from pathlib import Path
import random
import subprocess

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "AlencarSoftTVBOX-Clang/assets/fallback_particles.ts"
WIDTH, HEIGHT, FPS, SECONDS = 640, 360, 30, 30


def font(size):
    for candidate in (Path("C:/Windows/Fonts/arial.ttf"),
                      Path("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")):
        if candidate.exists():
            return ImageFont.truetype(str(candidate), size)
    return ImageFont.load_default()


def main():
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    rng = random.Random(6622)
    stars = [(rng.uniform(0, WIDTH), rng.uniform(0, HEIGHT),
              rng.uniform(0.4, 1.6), rng.uniform(1.5, 4.5)) for _ in range(48)]
    command = [
        "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
        "-f", "rawvideo", "-pixel_format", "rgb24", "-video_size", f"{WIDTH}x{HEIGHT}",
        "-framerate", str(FPS), "-i", "pipe:0",
        "-f", "lavfi", "-i", "anullsrc=channel_layout=stereo:sample_rate=44100",
        "-t", str(SECONDS), "-map", "0:v", "-map", "1:a",
        "-c:v", "libx264", "-preset", "veryfast", "-profile:v", "baseline",
        "-level:v", "3.0", "-pix_fmt", "yuv420p", "-g", "60",
        "-b:v", "400k", "-maxrate", "550k", "-bufsize", "1100k",
        "-c:a", "aac", "-b:a", "48k", "-ar", "44100", "-ac", "2",
        "-f", "mpegts", str(OUTPUT),
    ]
    process = subprocess.Popen(command, stdin=subprocess.PIPE)
    assert process.stdin is not None
    title_font, caption_font = font(27), font(15)
    try:
        for frame in range(FPS * SECONDS):
            t = frame / FPS
            image = Image.new("RGB", (WIDTH, HEIGHT), (5, 17, 34))
            draw = ImageDraw.Draw(image)
            for x, y, speed, radius in stars:
                phase = tau * t / SECONDS
                px = x + 20 * speed * sin(phase + x / WIDTH * tau)
                py = y + 8 * sin(phase + y / HEIGHT * tau)
                glow = int(90 + 70 * sin(phase + x / WIDTH * tau) ** 2)
                color = (12, min(255, glow + 50), min(255, glow + 95))
                draw.ellipse((px - radius, py - radius, px + radius, py + radius), fill=color)
            draw.text((WIDTH // 2, 155), "ALENCARSOFT TVBOX", fill=(225, 243, 255),
                      font=title_font, anchor="mm")
            draw.text((WIDTH // 2, 193), "Aguardando um canal disponivel", fill=(92, 198, 230),
                      font=caption_font, anchor="mm")
            process.stdin.write(image.tobytes())
    finally:
        process.stdin.close()
    if process.wait() != 0:
        raise SystemExit("ffmpeg failed to create the fallback TS")
    print(f"Generated {OUTPUT} ({OUTPUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Cut live QindaQt captures into character-led, captioned product films.

The footage comes from record-live.sh. Mascot art is cut from the wallpaper
identity illustration; only motion, focus rings and editorial labels are added.
"""

from __future__ import annotations

import argparse
import bisect
import json
import math
import subprocess
from dataclasses import dataclass
from pathlib import Path

import soundfile as sf
from kokoro_onnx import Kokoro
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
SITE = ROOT / "website"
LIVE = ROOT / "build/site-capture/live"
STAGE = ROOT / "build/site-capture/live-film"
OUTPUT = SITE / "assets/video"
ART = SITE / "assets/images/cyber-pengu-ducke.png"
REGULAR = "/usr/share/fonts/dejavu/DejaVuSans.ttf"
BOLD = "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf"
FPS = 12
WIDTH, HEIGHT = 1600, 900


@dataclass(frozen=True)
class Scene:
    capture: str
    title: str
    speaker: str
    spoken: str
    caption: str
    cue: str
    focus: tuple[int, int]


FILMS: dict[str, list[Scene]] = {
    "desktop-tour": [
        Scene("desktop-welcome", "START WITH THE FAMILIAR", "pengu",
              "Kinda Cute starts with ordinary windows. The welcome guide shows what changes when you bring related work together.",
              "QindaQt starts with ordinary windows. The welcome guide shows what changes when you bring related work together.",
              "Watch the windows", (368, 374)),
        Scene("desktop-windows", "WINDOWS, YOUR WAY", "ducke",
              "Settings and Files begin apart. Hold Meta and Shift, drag a title to the edge, and there it is: one live split. Both tools still work.",
              "Settings and Files begin apart. Hold Meta and Shift, drag a title to the edge, and there it is: one live split. Both tools still work.",
              "One drag. Two tools.", (691, 473)),
        Scene("desktop-settings", "CHANGE THE WHOLE LAYOUT", "pengu",
              "In Settings, I move from Menu and Dock to Classic Taskbar, then back. These are real presets. The panels can be edited and saved as your own layout.",
              "In Settings, I move from Menu and Dock to Classic Taskbar, then back. These are real presets. The panels can be edited and saved as your own layout.",
              "Make room to work", (887, 321)),
        Scene("desktop-files", "THE EVERYDAY TOOLS", "ducke",
              "The wallpaper collection is an ordinary folder in File Manager. Pick a scene, open your files, and keep the desktop you actually want to use.",
              "The wallpaper collection is an ordinary folder in File Manager. Pick a scene, open your files, and keep the desktop you actually want to use.",
              "Pick your backdrop", (890, 430)),
    ],
    "office-tour": [
        Scene("office-write", "WRITE THE BRIEF", "pengu",
              "Signal Garden begins in Kinda Write: goals, a timed schedule, and a Friday decision on an editable page. I add the deadline right in the brief.",
              "Signal Garden begins in QindaWrite: goals, a timed schedule, and a Friday decision on an editable page. I add the deadline right in the brief.",
              "Put the decision in writing", (676, 302)),
        Scene("office-calc", "TEST THE BUDGET", "ducke",
              "Watch the lighting quantity. I change one to two. The plan, total cost, and balance recalculate. We're left with only two hundred and thirty eight dollars.",
              "Watch the lighting quantity. I change one to two. The plan, total cost, and balance recalculate. We're left with only $238.",
              "One edit changes the plan", (547, 677)),
        Scene("office-chart", "COMPARE SCENARIOS", "pengu",
              "A second sheet compares a quiet night, our plan, and a full house. That chart comes from the actual workbook, not a picture pasted over it.",
              "A second sheet compares a quiet night, our plan, and a full house. That chart comes from the actual workbook, not a picture pasted over it.",
              "Three possible nights", (720, 482)),
        Scene("office-show", "MAKE THE CASE", "ducke",
              "Now the pitch. Four real slides move from the invitation to the decision. Speaker notes beside the last slide tell us exactly what approval to ask for.",
              "Now the pitch. Four real slides move from the invitation to the decision. Speaker notes beside the last slide tell us exactly what approval to ask for.",
              "The notes stay beside it", (1160, 316)),
        Scene("office-note", "KEEP THE FIELD WORK", "pengu",
              "The courtyard walkthrough and launch checklist live in Kinda Note. The step free route is marked done. The low glare night test is still waiting.",
              "The courtyard walkthrough and launch checklist live in QindaNote. The step-free route is marked done. The low-glare night test is still waiting.",
              "A checklist with context", (241, 298)),
    ],
}


def run(*args: str) -> None:
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL)


def timestamp(seconds: float) -> str:
    value = round(seconds * 1000)
    hours, value = divmod(value, 3_600_000)
    minutes, value = divmod(value, 60_000)
    whole, value = divmod(value, 1000)
    return f"{hours:02}:{minutes:02}:{whole:02}.{value:03}"


def mascot_parts() -> dict[str, Image.Image]:
    art = Image.open(ART).convert("RGBA")
    pengu = art.crop((355, 35, 1360, 1157))
    mask = pengu.getchannel("A")
    ImageDraw.Draw(mask).polygon([(0, 530), (165, 530), (135, 680),
                                  (150, 850), (100, 1122), (0, 1122)], fill=0)
    pengu.putalpha(mask)
    ducke = art.crop((0, 570, 530, 1125))
    mask = ducke.getchannel("A")
    ImageDraw.Draw(mask).polygon([(455, 0), (530, 0), (530, 120),
                                  (505, 120), (475, 70)], fill=0)
    ducke.putalpha(mask)
    return {"pengu": pengu, "ducke": ducke}


def add_character(canvas: Image.Image, art: Image.Image, *, x: int, y: int,
                  width: int, angle: float) -> None:
    height = round(art.height * width / art.width)
    pose = art.resize((width, height), Image.Resampling.LANCZOS)
    pose = pose.rotate(angle, Image.Resampling.BICUBIC, expand=True)
    canvas.alpha_composite(pose, (x, y))


def draw_frame(screen: Image.Image, scene: Scene, index: int, count: int,
               chapter: int, chapters: int, parts: dict[str, Image.Image]) -> Image.Image:
    progress = index / max(1, count - 1)
    canvas = Image.new("RGBA", (WIDTH, HEIGHT), "#102934")
    canvas.alpha_composite(screen.convert("RGBA"), (0, 0))
    draw = ImageDraw.Draw(canvas, "RGBA")
    draw.rectangle((1440, 0, WIDTH, HEIGHT), fill=(20, 49, 57, 255))
    draw.rectangle((1440, 0, 1444, HEIGHT), fill=(137, 220, 196, 255))
    for y in range(45, 850, 26):
        draw.line((1473, y, 1570, y), fill=(162, 221, 203, 24), width=1)
    font_small = ImageFont.truetype(BOLD, 17)
    font_title = ImageFont.truetype(BOLD, 24)
    font_body = ImageFont.truetype(REGULAR, 19)
    draw.text((1464, 42), "QINDAQT", font=font_small, fill="#a8e2cb")
    draw.text((1464, 72), f"{chapter:02} / {chapters:02}", font=font_small, fill="#f4e9c8")

    # Focus rings follow the real captured controls; they never replace UI.
    # Wait until the real input has produced the result, then draw attention to it.
    active = max(0.0, math.sin((progress - .58) / .42 * math.pi)) if progress > .58 else 0.0
    if active > .12:
        fx, fy = scene.focus
        radius = 28 + 8 * math.sin(index * .55)
        color = (142, 231, 203, round(150 * active)) if scene.speaker == "pengu" else (241, 168, 88, round(170 * active))
        draw.ellipse((fx-radius, fy-radius, fx+radius, fy+radius), outline=color, width=4)

    # The two separate cutouts move independently. The speaking host leans in;
    # the other reacts, and Ducke's fan gets short motion streaks.
    bounce = math.sin(index * .65)
    if scene.speaker == "pengu":
        add_character(canvas, parts["ducke"], x=1210, y=727 + round(bounce * 5),
                      width=145, angle=-4 + bounce * 3)
        add_character(canvas, parts["pengu"], x=1320, y=555 + round(bounce * 7),
                      width=280, angle=1.8 * bounce)
    else:
        add_character(canvas, parts["pengu"], x=1235, y=634 + round(bounce * 4),
                      width=210, angle=2 * bounce)
        add_character(canvas, parts["ducke"], x=1350, y=624 + round(bounce * 9),
                      width=238, angle=-3.5 * bounce)
        draw = ImageDraw.Draw(canvas, "RGBA")
        for offset in range(3):
            draw.arc((1350 - offset*9, 687 - offset*8, 1430 + offset*8, 768 + offset*9),
                     120 + index*18, 215 + index*18,
                     fill=(150, 231, 204, 120-offset*27), width=2)

    draw = ImageDraw.Draw(canvas, "RGBA")
    bubble = (1260, 380, 1584, 504)
    draw.rounded_rectangle(bubble, radius=18, fill=(239, 236, 218, 246),
                           outline=(30, 55, 61, 255), width=3)
    draw.polygon([(1454, 503), (1488, 503), (1473, 526)], fill=(239, 236, 218, 246))
    name = "CYBER PENGU" if scene.speaker == "pengu" else "DUCKE"
    draw.text((1281, 399), name, font=font_small, fill="#177b69" if scene.speaker == "pengu" else "#b65b2e")
    draw.text((1280, 433), scene.cue, font=font_body, fill="#172f3d")
    for bar in range(4):
        amplitude = 7 + (10 + bar * 3) * abs(math.sin(index*.45 + bar))
        draw.rounded_rectangle((1282 + bar*18, 481-amplitude, 1290 + bar*18, 481),
                               radius=3, fill="#d27439")

    if progress < .27:
        opacity = 225 if progress < .20 else round(225 * (.27-progress)/.07)
        draw.rounded_rectangle((26, 777, 763, 861), radius=10,
                               fill=(20, 49, 57, opacity),
                               outline=(168, 226, 203, opacity), width=2)
        draw.text((49, 802), scene.title, font=font_title,
                  fill=(250, 242, 217, opacity))
    return canvas.convert("RGB")


def synthesize(scene: Scene, audio_path: Path, kokoro: Kokoro) -> float:
    if not audio_path.exists():
        voice = "bm_george" if scene.speaker == "pengu" else "am_puck"
        language = "en-gb" if scene.speaker == "pengu" else "en-us"
        speed = .98 if scene.speaker == "pengu" else 1.06
        samples, rate = kokoro.create(scene.spoken, voice=voice, lang=language, speed=speed)
        sf.write(audio_path, samples, rate)
    samples, rate = sf.read(audio_path)
    return len(samples) / rate


def render_scene(scene: Scene, number: int, total: int, audio: Path,
                 film_stage: Path, parts: dict[str, Image.Image]) -> tuple[Path, float]:
    speech, rate = sf.read(audio)
    duration = len(speech) / rate + .8
    capture = LIVE / scene.capture
    times = json.loads((capture / "timing.json").read_text())
    frames = sorted(capture.glob("*.png"))
    assert frames and len(frames) == len(times), scene.capture
    count = math.ceil(duration * FPS)
    clip = film_stage / f"{number:02}.mp4"
    command = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
               "-f", "rawvideo", "-pixel_format", "rgb24", "-video_size", f"{WIDTH}x{HEIGHT}",
               "-framerate", str(FPS), "-i", "pipe:0", "-i", str(audio),
               "-af", "apad=pad_dur=0.8,highpass=f=70,lowpass=f=9500",
               "-frames:v", str(count), "-c:v", "libx264", "-preset", "veryfast",
               "-crf", "22", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "128k",
               "-movflags", "+faststart", str(clip)]
    process = subprocess.Popen(command, stdin=subprocess.PIPE)
    assert process.stdin is not None
    last_index = -1
    screen = None
    try:
        for frame_index in range(count):
            capture_time = (frame_index / FPS) / duration * times[-1]
            source_index = min(len(frames)-1, bisect.bisect_right(times, capture_time))
            if source_index != last_index:
                screen = Image.open(frames[source_index]).convert("RGB")
                last_index = source_index
            assert screen is not None
            frame = draw_frame(screen, scene, frame_index, count, number, total, parts)
            process.stdin.write(frame.tobytes())
    finally:
        process.stdin.close()
    if process.wait() != 0:
        raise RuntimeError(f"FFmpeg failed for {scene.capture}")
    return clip, count / FPS


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--voices", type=Path, required=True)
    parser.add_argument("--film", choices=FILMS)
    parser.add_argument("--audio-only", action="store_true")
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    STAGE.mkdir(parents=True, exist_ok=True)
    kokoro = Kokoro(str(args.model), str(args.voices))
    parts = mascot_parts()
    for film, scenes in FILMS.items():
        if args.film and film != args.film:
            continue
        film_stage = STAGE / film
        film_stage.mkdir(parents=True, exist_ok=True)
        clips = []
        captions = ["WEBVTT", ""]
        elapsed = 0.0
        for number, scene in enumerate(scenes, 1):
            audio_path = film_stage / f"{number:02}.wav"
            duration = synthesize(scene, audio_path, kokoro) + .8
            if args.audio_only:
                print(f"{film} / {scene.capture}: audio {duration:.1f}s", flush=True)
                continue
            clip, duration = render_scene(scene, number, len(scenes), audio_path,
                                          film_stage, parts)
            clips.append(clip)
            captions.extend([str(number),
                             f"{timestamp(elapsed)} --> {timestamp(elapsed + duration - .2)}",
                             scene.caption, ""])
            elapsed += duration
            print(f"{film} / {scene.capture}: {duration:.1f}s", flush=True)
        if args.audio_only:
            continue
        concat = film_stage / "clips.txt"
        concat.write_text("".join(f"file '{clip}'\n" for clip in clips))
        run("ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-f", "concat",
            "-safe", "0", "-i", str(concat), "-c", "copy", "-movflags", "+faststart",
            str(OUTPUT / f"{film}.mp4"))
        (OUTPUT / f"{film}.vtt").write_text("\n".join(captions), encoding="utf-8")
        print(f"Wrote {film}: {elapsed:.1f}s", flush=True)


if __name__ == "__main__":
    main()

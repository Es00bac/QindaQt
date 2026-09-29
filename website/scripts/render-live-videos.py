#!/usr/bin/env python3
"""Cut live QindaQt captures into character-led, captioned product films.

The footage comes from record-live.sh. Mascot art is cut from the wallpaper
identity illustration; the drawn host action frames now change during speech.
"""

from __future__ import annotations

import argparse
import bisect
import json
import math
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import soundfile as sf
from kokoro_onnx import Kokoro
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
SITE = ROOT / "website"
LIVE = ROOT / "build/site-capture/live"
STAGE = ROOT / "build/site-capture/live-film"
OUTPUT = SITE / "assets/video"
HOST_SHEET = SITE / "assets/images/hosts-animation-sheet.png"
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


def tour(capture: str, title: str, speaker: str, spoken: str,
         cue: str, focus: tuple[int, int]) -> Scene:
    return Scene(capture, title, speaker, spoken, spoken, cue, focus)


FILMS: dict[str, list[Scene]] = {
    "desktop-tour": [
        tour("desktop-welcome", "WHAT IS QINDAQT?", "pengu",
             "Kinda Cute, written QindaQt, is a free and open source Wayland desktop. You get a shell, settings, a file manager, native tools, and an optional office suite. This is a captured session, not a concept picture. The Welcome guide starts with the main idea: keep ordinary windows until you choose to make them work together.",
             "Start with ordinary windows", (368, 374)),
        tour("desktop-windows", "MAKE A WINDOW CONTAINER", "ducke",
             "Here are Files and Settings as separate, working windows. I hold Meta and Shift and drag Settings toward the left edge of Files. The preview tells me where it will land. Release, and they share one movable frame with a live split. I can drag the divider to give either app more room. Each member keeps working and still has its own title controls. The group menu handles detach, ungroup, and moving the whole group; closing the frame asks what to do with its members.",
             "An edge drop makes a split", (690, 480)),
        tour("desktop-tabs", "TABS ARE PAGES", "pengu",
             "The same gesture has a second answer. Drop near the middle instead of an edge, and Files and Settings become pages in one container. I switch between those real applications in the shared tab row. A page can itself contain splits, so a container can hold a larger project without turning every window into a tile. Pull a member's own title strip into empty space to detach it. You can also open the group menu, or use Meta Shift D and choose a target with arrows, T for a tab, or D to detach.",
             "A center drop makes tabs", (754, 165)),
        tour("desktop-settings", "CHANGE THE LAYOUT", "ducke",
             "Settings is where the desktop shape changes. In Customize I choose Classic Taskbar, then restore Menu and Dock. Those are two of eleven starting layouts. A layout changes the panels and applets; it does not erase the color theme I chose. Right click a panel for Edit Panels, then move items, add or duplicate applets, and save the result as your own preset. Your starting point is only a starting point.",
             "Eleven starting points", (888, 320)),
        tour("desktop-appearance", "ONE COMPLETE THEME", "pengu",
             "Appearance previews the real window chrome around ordinary application controls. I browse the theme cards and select Graphite. The draft preview changes before I apply it. A theme brings its palette, window and container decoration, title buttons, and button arrangement together. Fine tune this theme opens the details: button style and side, title height, corner shape, and container controls. Wallpaper and fonts have their own tabs. Apply commits the draft; Revert returns to the saved look.",
             "Preview before Apply", (1013, 510)),
        tour("layout-qindaqt", "01 / QINDAQT", "ducke",
             "QindaQt's signature arrangement has a thin top ledger, a left smart shelf, and compact instruments below. The File Manager still moves freely; panels never force a window layout.",
             "The signature arrangement", (25, 450)),
        tour("layout-qinda-bliss", "02 / CLASSIC TASKBAR", "pengu",
             "Classic Taskbar gathers launching, running windows, and status on one bar. Classic Blue and Qinda Bliss complete the familiar look, and every panel is still editable.",
             "Classic Blue and Bliss", (700, 870)),
        tour("layout-windows-modern", "03 / CENTERED TASKBAR", "ducke",
             "Centered Taskbar moves the launcher into the middle. Daylight controls and Porcelain Dawn match the modern shape, while QindaQt's window containers still work.",
             "Centered, not locked down", (700, 872)),
        tour("layout-beos-inspired", "04 / CORNER BAR", "pengu",
             "Corner Bar puts a compact panel in the corner and a short title tab on each window. Marigold, Sea Glass, and Lilac vary the color and corner radius. Clear space beside the tab lets clicks reach what is beneath.",
             "One layout, three cuts", (540, 160)),
        tour("layout-nextstep-inspired", "05 / WORKSPACE DOCK", "ducke",
             "Workspace Dock lines up on the right, inspired by NeXT. Graphite frames and Ink Tide match its quieter look. The dock opens a workspace; it does not dictate the windows within it.",
             "A dock on the right", (1400, 440)),
        tour("layout-win31-inspired", "06 / PROGRAM GROUPS", "pengu",
             "Program Groups revisits Windows three point one: grouped applications and windows that minimize to desktop icons. Classic Grey gives the profile square decoration. It is editable like the others.",
             "Groups and desktop icons", (700, 15)),
        tour("layout-macos-inspired", "07 / MENU AND DOCK", "ducke",
             "Menu and Dock is the shipped default. The active application's menu sits across the top and the dock below. Mist and Jade Fold give both parts a matching face.",
             "The shipped default", (700, 874)),
        tour("layout-gnome-inspired", "08 / OVERVIEW", "pengu",
             "Overview starts from workspaces and an Activities entry. Slate and Ink Tide give it a dark treatment. The same apps, settings, and window grouping remain available.",
             "Start from workspaces", (52, 18)),
        tour("layout-unity-inspired", "09 / COMMAND RAIL", "ducke",
             "Command Rail brings launch and switching controls down the left, with a search HUD above. Dusk and Qinda Punk match its vertical rhythm. Rearrange the rail for your apps.",
             "Launch from the side", (31, 445)),
        tour("layout-xfce-inspired", "10 / COMPACT FLEXIBLE", "pengu",
             "Compact Flexible uses a lean top panel and small launcher below. A light theme and Porcelain Dawn leave room for the work, with panels ready to change later.",
             "Leave room for work", (718, 877)),
        tour("layout-minimal", "11 / MINIMAL", "ducke",
             "Minimal leaves the stage to the application. Ink Tide and dark glass keep the frame quiet. Bring back any panel or applet when you need it.",
             "A quiet starting point", (710, 160)),
        tour("desktop-files", "NATIVE EVERYDAY TOOLS", "pengu",
             "File Manager is a native app. Browse folders, switch views, use bookmarks, and open the wallpaper collection shown here. Built-in tools extend from files into writing, commands, planning, media, and monitoring. The global menu can move supported app menus into the shell, with an in-window fallback.",
             "Files are part of the story", (894, 430)),
        tour("desktop-editor", "WRITE IN THE DESKTOP", "ducke",
             "QindaQt Text Editor opens a real runbook for our sample event. I add the owner for the night test and save it. Under the simple interface are find and replace, syntax highlighting, atomic saves, and warnings if the file changed outside the editor. Its application menu is visible in the top panel here.",
             "Edit and save a real runbook", (632, 271)),
        tour("desktop-terminal", "COMMANDS BELONG HERE TOO", "pengu",
             "QQ Term is the native terminal. Here I list the actual sample files used by the Office demonstration. Tabs, profiles, and color schemes make it comfortable for longer work. When a global menu is available, the terminal can use it too.",
             "Inspect the sample project", (664, 395)),
        tour("desktop-calendar", "PLAN THE WORK", "ducke",
             "Calendar is a local first-party app with month, week, and day views. Create or change events, import and export I C S, and get reminders while the app is running. This build does not yet claim network calendar sync or a background reminder service.",
             "A local calendar", (547, 214)),
        tour("desktop-mail", "MAIL JOINS THE WORKFLOW", "pengu",
             "QindaMail belongs to the companion Office project. It brings mail, calendar, and tasks into one app, with an on-computer account for local work and optional network accounts. It is still in development, like the rest of this environment.",
             "Mail, calendar, and tasks", (680, 321)),
        tour("desktop-voice", "OPTIONAL VOICE INPUT", "ducke",
             "The Voice page makes dictation an explicit choice. Pick and configure a provider, review whether it runs locally or sends audio to a cloud service, then try speaking into a focused app. Without a configured provider it stays unavailable; QindaQt does not pretend a microphone is ready when it is not.",
             "Opt in to dictation", (718, 332)),
        tour("audio-mixer", "A REAL AUDIO ENGINE", "ducke",
             "Audio is more than a volume slider. Audio One has five physical and three virtual input strips, five hardware buses and three virtual buses. You can set each send independently, watch live meters, use processing racks, record, and configure manual stereo streams to another computer. In this captured example a test tone enters Virtual Input, I enable its B one send, and the B one meter responds in an isolated PipeWire graph.",
             "Virtual Input to B1", (663, 330)),
        tour("office-calc", "OFFICE WORK THAT MOVES", "pengu",
             "The companion Office suite handles real work too. This budget is an editable workbook for Signal Garden, a sample neighborhood event. I double the lighting quantity and the formula raises cost above the project's ceiling. Then I return it to one before the pitch. The related brief, chart, slide deck, and field notes open in QindaWrite, QindaCalc, QindaShow, and QindaNote. You can download the same files from the Office site.",
             "A decision in a real workbook", (551, 676)),
    ],
    "office-tour": [
        Scene("office-write", "WRITE THE BRIEF", "pengu",
              "Signal Garden begins in Kinda Write: goals, a timed schedule, and a Friday decision on an editable page. I add the deadline right in the brief.",
              "Signal Garden begins in QindaWrite: goals, a timed schedule, and a Friday decision on an editable page. I add the deadline right in the brief.",
              "Put the decision in writing", (676, 302)),
        Scene("office-calc", "TEST THE BUDGET", "ducke",
              "Watch the lighting quantity. I change one to two, and total cost jumps to nine thousand eight hundred and eighty two dollars. That's over our eight thousand six hundred dollar ceiling. I put it back to one. The affordable plan and two thousand four hundred and eighty eight dollar balance return before the pitch.",
              "Lighting 1 → 2 raises cost to $9,882, over the $8,600 ceiling. I revert to 1; the $7,632 plan and $2,488 balance return before the pitch.",
              "Test it. Reject it. Revert.", (547, 677)),
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
    "audio-tour": [
        Scene("audio-devices", "START WITH REAL DEVICES", "pengu",
              "The Audio page begins with devices and application streams. Here I open Mixer. It is a real console, with five hardware and three virtual input strips.",
              "The Audio page begins with devices and application streams. Here I open Mixer: a real console with five hardware and three virtual input strips.",
              "Open the console", (846, 245)),
        Scene("audio-mixer", "TRACE THE MIX", "ducke",
              "Here a test tone enters Virtual Input. Watch its live meter. I turn on the B1 send, and the signal reaches a separate virtual output bus. The route is real, made inside a private PipeWire graph, with no hardware speaker connected.",
              "A real test tone enters Virtual Input. I enable its B1 send and the live signal reaches a virtual output bus in an isolated PipeWire graph.",
              "Route it to B1", (663, 330)),
        Scene("audio-peers", "REACH ANOTHER COMPUTER", "pengu",
              "Other computers is where you configure a manual stereo peer. Save a destination and bus, review the receive permission, then explicitly enable the stream. An active local route still needs an audible check at the far end.",
              "Other computers configures a manual stereo peer. Save the route, review receive permission, then enable it. Check audibility at the far end.",
              "A deliberate connection", (1092, 245)),
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


def timed_captions(transcript: str, start: float, duration: float,
                   first_number: int) -> list[str]:
    # Long product chapters need readable, advancing captions. Kokoro does not
    # publish word timestamps, so allocate each short phrase its share of the
    # spoken duration and hold the last phrase through the closing beat.
    words = transcript.split()
    if not words:
        return []
    chunks: list[list[str]] = []
    remaining = words[:]
    while remaining:
        limit = min(12, len(remaining))
        stop = next((index for index in range(limit - 1, max(2, limit - 5), -1)
                     if re.search(r"[.!?;:]$", remaining[index])), None)
        take = (stop + 1) if stop is not None else limit
        chunks.append(remaining[:take])
        remaining = remaining[take:]
    spoken_duration = max(.1, duration - .8)
    entries: list[str] = []
    used = 0
    for index, chunk in enumerate(chunks):
        begin = start + spoken_duration * used / len(words)
        used += len(chunk)
        end = start + spoken_duration * used / len(words)
        if index == len(chunks) - 1:
            end = start + duration - .2
        entries.extend([str(first_number + index),
                        f"{timestamp(begin)} --> {timestamp(end)}",
                        " ".join(chunk), ""])
    return entries


def mascot_frames() -> list[Image.Image]:
    # AGENT-NOTE: This generated sheet is eight deliberately drawn animation
    # frames with changing beaks, wings, eyes, tablet and fan. The website uses
    # the same sheet. Keep exact four-by-two cell order across both renderers.
    sheet = Image.open(HOST_SHEET).convert("RGBA")
    frames = []
    for row in range(2):
        for column in range(4):
            bounds = (round(column * sheet.width / 4),
                      round(row * sheet.height / 2),
                      round((column + 1) * sheet.width / 4),
                      round((row + 1) * sheet.height / 2))
            frames.append(sheet.crop(bounds).resize((335, 335), Image.Resampling.LANCZOS))
    return frames


def draw_frame(screen: Image.Image, scene: Scene, index: int, count: int,
               chapter: int, chapters: int, poses: list[Image.Image],
               voice_level: float) -> Image.Image:
    progress = index / max(1, count - 1)
    canvas = Image.new("RGBA", (WIDTH, HEIGHT), "#192c30")
    canvas.alpha_composite(screen.convert("RGBA"), (0, 0))
    draw = ImageDraw.Draw(canvas, "RGBA")
    draw.rectangle((1440, 0, WIDTH, HEIGHT), fill=(233, 227, 212, 255))
    draw.rectangle((1440, 0, 1444, HEIGHT), fill=(25, 44, 48, 255))
    for y in range(45, 850, 26):
        draw.line((1473, y, 1570, y), fill=(25, 44, 48, 38), width=1)
    font_small = ImageFont.truetype(BOLD, 17)
    font_title = ImageFont.truetype(BOLD, 24)
    font_body = ImageFont.truetype(REGULAR, 19)
    draw.text((1464, 42), "QINDAQT", font=font_small, fill="#192c30")
    draw.text((1464, 72), f"{chapter:02} / {chapters:02}", font=font_small, fill="#cc4e2d")

    # Focus rings follow the real captured controls; they never replace UI.
    # Wait until the real input has produced the result, then draw attention to it.
    active = max(0.0, math.sin((progress - .58) / .42 * math.pi)) if progress > .58 else 0.0
    if active > .12:
        fx, fy = scene.focus
        radius = 28 + 8 * math.sin(index * .55)
        color = (142, 231, 203, round(150 * active)) if scene.speaker == "pengu" else (241, 168, 88, round(170 * active))
        draw.ellipse((fx-radius, fy-radius, fx+radius, fy+radius), outline=color, width=4)

    # Character animation runs through drawn in-between actions while the
    # voice is active; pauses settle to rest and the pointed control gets a
    # dedicated reaction frame. The artwork itself changes, not just its XY.
    frame_number = ((index // 3 + (2 if scene.speaker == "ducke" else 0)) % 8) \
        if voice_level > .12 else (6 if .63 < progress < .78 else 0)
    entrance = min(0, round((progress - .13) * 60))
    canvas.alpha_composite(poses[frame_number], (1240 - entrance, 565))

    draw = ImageDraw.Draw(canvas, "RGBA")
    bubble = (1260, 380, 1584, 504)
    draw.rectangle(bubble, fill=(232, 173, 52, 250),
                   outline=(25, 44, 48, 255), width=3)
    draw.polygon([(1454, 503), (1488, 503), (1473, 526)], fill=(232, 173, 52, 250))
    name = "CYBER PENGU" if scene.speaker == "pengu" else "DUCKE"
    draw.text((1281, 399), name, font=font_small, fill="#192c30")
    draw.text((1280, 433), scene.cue, font=font_body, fill="#192c30")
    for bar in range(4):
        amplitude = 7 + (12 + bar * 3) * voice_level
        draw.rectangle((1282 + bar*18, 481-amplitude, 1290 + bar*18, 481), fill="#cc4e2d")

    if progress < .27:
        opacity = 225 if progress < .20 else round(225 * (.27-progress)/.07)
        draw.rectangle((26, 777, 763, 861),
                       fill=(25, 44, 48, opacity),
                       outline=(232, 173, 52, opacity), width=2)
        draw.text((49, 802), scene.title, font=font_title,
                  fill=(250, 242, 217, opacity))
    return canvas.convert("RGB")


def synthesize(scene: Scene, audio_path: Path, kokoro: Kokoro) -> float:
    if not audio_path.exists():
        voice = "bm_george" if scene.speaker == "pengu" else "am_puck"
        language = "en-gb" if scene.speaker == "pengu" else "en-us"
        speed = .96 if scene.speaker == "pengu" else .92
        # Generate one sentence at a time. Long single calls can rush or
        # truncate the explanation; short phrasing gives each host a natural
        # pause and lets the full informational script be heard.
        pieces = []
        rate = 24_000
        for sentence in re.split(r"(?<=[.!?])\s+", scene.spoken):
            if not sentence.strip():
                continue
            samples, rate = kokoro.create(sentence, voice=voice,
                                          lang=language, speed=speed)
            pieces.extend((samples, np.zeros(int(rate * .18), dtype=samples.dtype)))
        sf.write(audio_path, np.concatenate(pieces), rate)
    samples, rate = sf.read(audio_path)
    return len(samples) / rate


def render_scene(scene: Scene, number: int, total: int, audio: Path,
                 film_stage: Path, poses: list[Image.Image]) -> tuple[Path, float]:
    speech, rate = sf.read(audio)
    duration = len(speech) / rate + .8
    capture = LIVE / scene.capture
    times = json.loads((capture / "timing.json").read_text())
    frames = sorted(capture.glob("*.png"))
    assert frames and len(frames) == len(times), scene.capture
    count = math.ceil(duration * FPS)
    levels = []
    for frame_index in range(count):
        start = int(frame_index / FPS * rate)
        end = min(len(speech), int((frame_index + 1) / FPS * rate))
        segment = speech[start:end]
        levels.append(float((sum(segment * segment) / max(1, len(segment))) ** .5))
    peak_level = max(levels, default=1) or 1
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
            frame = draw_frame(screen, scene, frame_index, count, number, total,
                               poses, levels[frame_index] / peak_level)
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
    poses = mascot_frames()
    for film, scenes in FILMS.items():
        if args.film and film != args.film:
            continue
        film_stage = STAGE / film
        film_stage.mkdir(parents=True, exist_ok=True)
        clips = []
        captions = ["WEBVTT", ""]
        caption_number = 1
        elapsed = 0.0
        for number, scene in enumerate(scenes, 1):
            audio_path = film_stage / f"{number:02}.wav"
            duration = synthesize(scene, audio_path, kokoro) + .8
            if args.audio_only:
                print(f"{film} / {scene.capture}: audio {duration:.1f}s", flush=True)
                continue
            clip, duration = render_scene(scene, number, len(scenes), audio_path,
                                          film_stage, poses)
            clips.append(clip)
            entries = timed_captions(scene.caption, elapsed, duration, caption_number)
            captions.extend(entries)
            caption_number += len(entries) // 4
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

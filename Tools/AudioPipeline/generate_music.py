#!/usr/bin/env python3
"""
Shattered Horizon 2032 — AI Music Pipeline
Primordia Games — Automated Soundtrack Generation

Usage:
    # Generate all music tracks
    python generate_music.py --full

    # Generate a single category
    python generate_music.py --category ambient
    python generate_music.py --category combat
    python generate_music.py --category stingers
    python generate_music.py --category menu

    # Generate a single track by ID
    python generate_music.py --track main_menu_theme

    # Specify output directory
    python generate_music.py --full --output /path/to/output

    # Process-only mode: skip generation, just process existing raw files
    python generate_music.py --full --skip-generation

Environment Variables (set before running):
    SUNO_API_KEY         — Suno API key for music generation
    STABILITY_API_KEY    — Stability AI key for Stable Audio generation
"""

import argparse
import json
import sys
from pathlib import Path

# Add parent to path for module imports
sys.path.insert(0, str(Path(__file__).parent))

from modules.music_generator import MusicGenerator
from modules.music_processor import (
    normalize_loudness,
    create_loop_points,
    fade_in_out,
    trim_to_duration,
    export_wav,
)


def load_manifest(manifest_path: Path) -> dict:
    """Load and validate the music manifest."""
    if not manifest_path.exists():
        print(f"Error: Manifest not found: {manifest_path}")
        sys.exit(1)

    with open(manifest_path) as f:
        manifest = json.load(f)

    tracks = manifest.get("tracks", [])
    if not tracks:
        print("Error: No tracks defined in manifest")
        sys.exit(1)

    print(f"Loaded manifest: {len(tracks)} tracks")
    categories = {}
    for t in tracks:
        cat = t.get("category", "unknown")
        categories[cat] = categories.get(cat, 0) + 1
    for cat, count in sorted(categories.items()):
        print(f"  {cat}: {count} tracks")

    return manifest


def filter_tracks(manifest: dict, category: str = None,
                  track_id: str = None) -> dict:
    """Filter manifest tracks by category or single track ID."""
    tracks = manifest.get("tracks", [])

    if track_id:
        filtered = [t for t in tracks if t["id"] == track_id]
        if not filtered:
            available = [t["id"] for t in tracks]
            print(f"Error: Track '{track_id}' not found.")
            print(f"Available tracks: {', '.join(available)}")
            sys.exit(1)
        tracks = filtered

    elif category:
        filtered = [t for t in tracks if t["category"] == category]
        if not filtered:
            available = sorted(set(t["category"] for t in tracks))
            print(f"Error: Category '{category}' not found.")
            print(f"Available categories: {', '.join(available)}")
            sys.exit(1)
        tracks = filtered

    result = dict(manifest)
    result["tracks"] = tracks
    return result


def process_track(raw_path: Path, track_config: dict,
                  output_dir: Path, manifest_meta: dict) -> Path:
    """Run the full processing chain on a single generated track.

    Pipeline: normalize -> trim -> loop/fade -> export
    """
    track_id = track_config["id"]
    category = track_config.get("category", "unknown")
    target_sr = manifest_meta.get("default_sample_rate", 48000)
    target_bits = manifest_meta.get("default_bit_depth", 16)

    # Working copy in a temp processing directory
    proc_dir = output_dir / "_processing"
    proc_dir.mkdir(parents=True, exist_ok=True)
    work_path = proc_dir / f"{track_id}_work.wav"

    # Copy raw to working path (via re-export to ensure consistent format)
    export_wav(raw_path, work_path, sample_rate=target_sr, bit_depth=target_bits)

    # Step 1: Normalize loudness
    # Use -14 LUFS for music (streaming standard)
    normalize_loudness(work_path, target_lufs=-14.0)

    # Step 2: Trim to target duration
    target_duration = track_config.get("duration_seconds")
    if target_duration:
        trim_to_duration(work_path, target_seconds=target_duration)

    # Step 3: Loop points or fades depending on track type
    if track_config.get("loop", False):
        crossfade_ms = track_config.get("crossfade_ms", 3000)
        if crossfade_ms > 0:
            create_loop_points(work_path, crossfade_ms=crossfade_ms)
    else:
        # Non-looping tracks get fade-in/out
        duration = track_config.get("duration_seconds", 10)
        fade_in = min(200, int(duration * 1000 * 0.05))
        fade_out = min(1000, int(duration * 1000 * 0.15))
        fade_in_out(work_path, fade_in_ms=fade_in, fade_out_ms=fade_out)

    # Step 4: Final export to category subdirectory
    category_dir = output_dir / category
    category_dir.mkdir(parents=True, exist_ok=True)
    final_path = category_dir / f"MUS_{track_id}.wav"

    export_wav(work_path, final_path, sample_rate=target_sr,
               bit_depth=target_bits)

    # Clean up working file
    work_path.unlink(missing_ok=True)

    return final_path


def run_pipeline(manifest: dict, output_dir: Path,
                 skip_generation: bool = False) -> dict:
    """Run the full music pipeline: generate -> process -> export.

    Returns dict of {track_id: final_path}.
    """
    meta = manifest.get("meta", {})
    tracks = manifest.get("tracks", [])
    results = {}

    print(f"\n{'=' * 60}")
    print(f"  Shattered Horizon 2032 — Music Pipeline")
    print(f"  Tracks to process: {len(tracks)}")
    print(f"  Output: {output_dir}")
    print(f"{'=' * 60}\n")

    # Phase 1: Generation
    generated_paths = {}
    if not skip_generation:
        generator = MusicGenerator(output_dir)
        generated_paths = generator.generate_all(manifest)
    else:
        # Look for existing raw files
        raw_dir = output_dir / "_raw"
        if raw_dir.exists():
            for track in tracks:
                tid = track["id"]
                # Search for any matching raw file
                for pattern in [f"*{tid}*", f"suno_*", f"stable_*"]:
                    matches = list(raw_dir.glob(pattern))
                    if matches:
                        generated_paths[tid] = matches[0]
                        break
        print(f"[Pipeline] Found {len(generated_paths)} existing raw files")

    # Phase 2: Processing
    print(f"\n{'=' * 60}")
    print(f"  Phase 2: Post-Processing")
    print(f"{'=' * 60}\n")

    for track in tracks:
        tid = track["id"]
        raw_path = generated_paths.get(tid)

        if raw_path and Path(raw_path).exists():
            print(f"\nProcessing: {tid}")
            try:
                final = process_track(Path(raw_path), track, output_dir, meta)
                results[tid] = final
                print(f"  -> {final}")
            except Exception as e:
                print(f"  ERROR processing {tid}: {e}")
                results[tid] = None
        else:
            print(f"\nSkipping {tid}: no raw audio available")
            results[tid] = None

    # Summary
    print(f"\n{'=' * 60}")
    print(f"  Pipeline Complete")
    print(f"{'=' * 60}")

    succeeded = sum(1 for v in results.values() if v is not None)
    failed = sum(1 for v in results.values() if v is None)
    print(f"  Succeeded: {succeeded}")
    print(f"  Failed/Skipped: {failed}")
    print(f"  Output: {output_dir}\n")

    # Write results manifest
    results_manifest = {
        "generated_tracks": {
            tid: str(path) if path else None
            for tid, path in results.items()
        }
    }
    results_path = output_dir / "music_results.json"
    with open(results_path, "w") as f:
        json.dump(results_manifest, f, indent=2)
    print(f"  Results written to: {results_path}")

    return results


def main():
    parser = argparse.ArgumentParser(
        description="SH2032 AI Music Pipeline — Primordia Games",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )

    parser.add_argument("--full", action="store_true",
                        help="Generate all music tracks")
    parser.add_argument("--category", type=str,
                        choices=["ambient", "combat", "stingers", "menu"],
                        help="Generate a single category of tracks")
    parser.add_argument("--track", type=str,
                        help="Generate a single track by ID")
    parser.add_argument("--output", type=str, default=None,
                        help="Output directory (default: AudioPipeline/output/music)")
    parser.add_argument("--skip-generation", action="store_true",
                        help="Skip AI generation, process existing raw files only")

    args = parser.parse_args()

    # Paths
    pipeline_dir = Path(__file__).parent
    manifest_path = pipeline_dir / "config" / "music_manifest.json"
    output_dir = Path(args.output) if args.output else pipeline_dir / "output" / "music"
    output_dir.mkdir(parents=True, exist_ok=True)

    # Load manifest
    manifest = load_manifest(manifest_path)

    # Filter if needed
    if args.track:
        manifest = filter_tracks(manifest, track_id=args.track)
    elif args.category:
        manifest = filter_tracks(manifest, category=args.category)
    elif not args.full:
        parser.print_help()
        print("\nQuick start:")
        print("  python generate_music.py --full                  # All tracks")
        print("  python generate_music.py --category ambient      # Ambient only")
        print("  python generate_music.py --track main_menu_theme # Single track")
        return

    # Run pipeline
    run_pipeline(manifest, output_dir, skip_generation=args.skip_generation)


if __name__ == "__main__":
    main()

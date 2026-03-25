#!/usr/bin/env python3
"""
Shattered Horizon 2032 — AI Art Pipeline
Primordia Games — Automated Asset Generation

Usage:
    # Full pipeline — source photos, AI generate, process PBR, export UE5
    python generate.py --full

    # Only source reference photos from APIs
    python generate.py --source-only

    # Process a single terrain type
    python generate.py --terrain concrete

    # Skip API sourcing, use existing reference photos
    python generate.py --full --skip-sourcing

    # Skip AI generation, just process references into PBR
    python generate.py --full --skip-ai

    # Process a local photo into a PBR texture set
    python generate.py --photo path/to/photo.jpg --name my_texture

    # Process all photos in reference_photos/ directory
    python generate.py --process-local

Environment Variables (set before running):
    UNSPLASH_ACCESS_KEY  — Unsplash API key (free at unsplash.com/developers)
    PEXELS_API_KEY       — Pexels API key (free at pexels.com/api)
    STABILITY_API_KEY    — Stability AI key (stability.ai)
    REPLICATE_API_TOKEN  — Replicate key (replicate.com) [optional]
"""

import argparse
import sys
from pathlib import Path

# Add parent to path for module imports
sys.path.insert(0, str(Path(__file__).parent))

from modules.orchestrator import PipelineOrchestrator
from modules.texture_processor import TextureProcessor


def main():
    parser = argparse.ArgumentParser(
        description="SH2032 AI Art Pipeline — Primordia Games",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )

    parser.add_argument("--full", action="store_true",
                        help="Run the full pipeline (source → AI → PBR → UE5)")
    parser.add_argument("--source-only", action="store_true",
                        help="Only source reference photos from APIs")
    parser.add_argument("--terrain", type=str,
                        help="Process a single terrain type by ID")
    parser.add_argument("--skip-sourcing", action="store_true",
                        help="Skip API image sourcing")
    parser.add_argument("--skip-ai", action="store_true",
                        help="Skip AI generation step")
    parser.add_argument("--photo", type=str,
                        help="Process a single local photo into PBR textures")
    parser.add_argument("--name", type=str, default="custom_texture",
                        help="Asset name for --photo mode")
    parser.add_argument("--process-local", action="store_true",
                        help="Process all images in reference_photos/ directory")
    parser.add_argument("--output", type=str, default=None,
                        help="Output directory (default: Tools/ArtPipeline/output)")

    args = parser.parse_args()

    # Paths
    pipeline_dir = Path(__file__).parent
    config_dir = str(pipeline_dir / "config")
    output_dir = args.output or str(pipeline_dir / "output")

    if args.photo:
        # Quick mode: process a single photo into PBR textures
        manifest_path = str(pipeline_dir / "config" / "asset_manifest.json")
        processor = TextureProcessor(manifest_path)

        photo_path = Path(args.photo)
        if not photo_path.exists():
            print(f"Error: Photo not found: {photo_path}")
            sys.exit(1)

        out = Path(output_dir) / "pbr" / args.name
        print(f"Processing {photo_path.name} → PBR set '{args.name}'")

        # Make seamless
        from PIL import Image
        img = Image.open(photo_path).convert("RGB")
        img = img.resize((2048, 2048), Image.LANCZOS)
        img = processor.make_seamless(img)

        seamless_path = Path(output_dir) / "seamless" / f"{args.name}.png"
        seamless_path.parent.mkdir(parents=True, exist_ok=True)
        img.save(seamless_path, "PNG")

        results = processor.process_full_pbr_set(seamless_path, out, args.name)
        print(f"\nGenerated {len(results)} PBR maps:")
        for map_type, path in results.items():
            print(f"  {map_type}: {path}")
        return

    if args.process_local:
        # Process all images in reference_photos directory
        manifest_path = str(pipeline_dir / "config" / "asset_manifest.json")
        processor = TextureProcessor(manifest_path)
        ref_dir = pipeline_dir / "reference_photos"

        if not ref_dir.exists() or not list(ref_dir.iterdir()):
            print(f"No images found in {ref_dir}")
            print("Place .jpg or .png files there and re-run.")
            sys.exit(1)

        for img_path in sorted(ref_dir.glob("*")):
            if img_path.suffix.lower() in (".jpg", ".jpeg", ".png", ".tif", ".tiff", ".bmp"):
                name = img_path.stem
                out = Path(output_dir) / "pbr" / name
                print(f"\nProcessing: {img_path.name}")

                from PIL import Image
                img = Image.open(img_path).convert("RGB")
                img = img.resize((2048, 2048), Image.LANCZOS)
                img = processor.make_seamless(img)

                seamless_path = Path(output_dir) / "seamless" / f"{name}.png"
                seamless_path.parent.mkdir(parents=True, exist_ok=True)
                img.save(seamless_path, "PNG")

                processor.process_full_pbr_set(seamless_path, out, name)
        return

    # Orchestrated pipeline modes
    pipeline = PipelineOrchestrator(config_dir, output_dir)

    if args.source_only:
        pipeline.run_references_only()
    elif args.terrain:
        pipeline.run_full_pipeline(
            skip_sourcing=args.skip_sourcing,
            skip_ai=args.skip_ai,
            terrain_ids=[args.terrain],
        )
    elif args.full:
        pipeline.run_full_pipeline(
            skip_sourcing=args.skip_sourcing,
            skip_ai=args.skip_ai,
        )
    else:
        parser.print_help()
        print("\nQuick start:")
        print("  python generate.py --full          # Run everything")
        print("  python generate.py --photo img.jpg  # Process one photo")


if __name__ == "__main__":
    main()

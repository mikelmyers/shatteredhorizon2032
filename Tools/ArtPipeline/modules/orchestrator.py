"""
Pipeline Orchestrator — End-to-end automated art asset generation.

Chains together all pipeline stages:
  1. Source reference photos via APIs (or use local references)
  2. Generate AI textures using reference + prompts
  3. Process into PBR texture sets (normal, roughness, AO, height)
  4. Make textures seamlessly tileable
  5. Color-grade to SH2032 milsim palette
  6. Export with UE5 naming conventions and material configs
  7. Generate import scripts and data tables

Designed for minimal human-in-the-loop operation.
"""

import json
import time
from pathlib import Path
from typing import Optional

from .image_sourcer import ImageSourcer
from .ai_generator import AIGenerator
from .texture_processor import TextureProcessor
from .ue5_exporter import UE5Exporter


class PipelineOrchestrator:
    """Runs the full art pipeline from sourcing to UE5-ready export."""

    def __init__(self, config_dir: str, output_dir: str):
        self.config_dir = Path(config_dir)
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)

        self.manifest_path = str(self.config_dir / "asset_manifest.json")
        self.api_config_path = str(self.config_dir / "api_config.json")

        with open(self.manifest_path) as f:
            self.manifest = json.load(f)

        # Initialize pipeline modules
        self.sourcer = ImageSourcer(self.api_config_path,
                                     str(self.output_dir / "reference_photos"))
        self.ai_gen = AIGenerator(self.api_config_path)
        self.processor = TextureProcessor(self.manifest_path)
        self.exporter = UE5Exporter(self.manifest_path)

        # Track all generated assets for final export
        self.generated_assets: list[dict] = []

    def run_full_pipeline(self, skip_sourcing: bool = False,
                           skip_ai: bool = False,
                           terrain_ids: Optional[list[str]] = None):
        """
        Run the complete pipeline for all terrain types.

        Args:
            skip_sourcing: Skip API image sourcing (use existing references)
            skip_ai: Skip AI generation (process references directly)
            terrain_ids: Only process these terrain IDs (None = all)
        """
        print("=" * 60)
        print("SHATTERED HORIZON 2032 — AI ART PIPELINE")
        print("Primordia Games — Automated Asset Generation")
        print("=" * 60)

        terrains = self.manifest["terrain_types"]
        if terrain_ids:
            terrains = [t for t in terrains if t["id"] in terrain_ids]

        total = len(terrains)
        stages = len(self.manifest.get("destruction_stages", []))

        print(f"\nProcessing {total} terrain types × {stages} destruction stages")
        print(f"= {total * stages} texture sets × 5 PBR maps = {total * stages * 5} textures")
        print()

        # Stage 1: Source reference photos
        reference_map = {}
        if not skip_sourcing:
            print("[Stage 1/5] Sourcing reference photos...")
            reference_map = self.sourcer.source_terrain_references(self.manifest_path)
            print(f"  Sourced references for {len(reference_map)} terrain types\n")
        else:
            print("[Stage 1/5] Skipping sourcing (using existing references)\n")
            reference_map = self._find_existing_references()

        # Stage 2: AI texture generation
        ai_outputs: dict[str, list[Path]] = {}
        if not skip_ai:
            print("[Stage 2/5] Generating AI textures...")
            for terrain in terrains:
                tid = terrain["id"]
                ref_image = None
                if tid in reference_map and reference_map[tid]:
                    ref_image = reference_map[tid][0]
                    print(f"  Using reference: {ref_image.name}")

                ai_dir = self.output_dir / "ai_raw" / tid
                outputs = self.ai_gen.generate_terrain_set(
                    terrain, self.manifest, ai_dir, ref_image
                )
                ai_outputs[tid] = outputs
                time.sleep(1)  # Rate limit
            print()
        else:
            print("[Stage 2/5] Skipping AI generation\n")
            ai_outputs = self._find_existing_ai_outputs()

        # Stage 3: Process into PBR texture sets
        print("[Stage 3/5] Processing PBR texture sets...")
        pbr_results: dict[str, list[dict]] = {}

        for terrain in terrains:
            tid = terrain["id"]
            pbr_results[tid] = []

            # Determine source images (AI outputs or references)
            sources = ai_outputs.get(tid, [])
            if not sources and tid in reference_map:
                sources = reference_map.get(tid, [])

            if not sources:
                print(f"  WARNING: No source images for {tid}, skipping")
                continue

            for i, source_path in enumerate(sources):
                stage_name = f"{tid}_stage{i}"
                pbr_dir = self.output_dir / "pbr" / tid

                # Make seamless first
                from PIL import Image
                img = Image.open(source_path).convert("RGB")
                size = tuple(self.manifest["texture_sizes"]["terrain"])
                img = img.resize(size, Image.LANCZOS)
                img = self.processor.make_seamless(img)

                seamless_path = self.output_dir / "seamless" / f"{stage_name}.png"
                seamless_path.parent.mkdir(parents=True, exist_ok=True)
                img.save(seamless_path, "PNG")

                # Generate PBR set
                pbr_maps = self.processor.process_full_pbr_set(
                    seamless_path, pbr_dir, stage_name
                )
                pbr_results[tid].append(pbr_maps)

        print()

        # Stage 4: Export to UE5 format
        print("[Stage 4/5] Exporting to UE5 format...")
        export_dir = self.output_dir / "ue5_export"

        for terrain in terrains:
            tid = terrain["id"]
            stage_maps = pbr_results.get(tid, [])

            for i, pbr_maps in enumerate(stage_maps):
                asset_name = f"{tid}_stage{i}"
                asset_meta = self.exporter.export_texture_set(
                    pbr_maps, asset_name, export_dir, "Terrain"
                )

                # Generate material config
                self.exporter.generate_material_config(
                    asset_name, "Terrain", asset_meta["textures"],
                    export_dir / "Terrain" / asset_name
                )

                self.generated_assets.append(asset_meta)

            # Generate destruction stage table
            if stage_maps:
                self.exporter.generate_destruction_material_table(
                    tid, self.generated_assets[-len(stage_maps):],
                    export_dir / "DataTables"
                )

        print()

        # Stage 5: Generate final export artifacts
        print("[Stage 5/5] Generating export artifacts...")
        tables_dir = export_dir / "DataTables"
        tables_dir.mkdir(parents=True, exist_ok=True)

        # Terrain data table
        terrain_assets = [a for a in self.generated_assets if "stage0" in a["asset_name"]]
        self.exporter.generate_terrain_data_table(terrain_assets, tables_dir)

        # Batch import script
        self.exporter.generate_batch_import_script(self.generated_assets, export_dir)

        # Pipeline report
        self._write_report(export_dir)

        print()
        print("=" * 60)
        print("PIPELINE COMPLETE")
        print(f"  Total assets generated: {len(self.generated_assets)}")
        print(f"  Output directory: {self.output_dir}")
        print(f"  UE5 export: {export_dir}")
        print("=" * 60)

    def run_single_terrain(self, terrain_id: str, reference_image: Optional[str] = None):
        """Quick run: generate assets for a single terrain type."""
        self.run_full_pipeline(
            skip_sourcing=reference_image is not None,
            terrain_ids=[terrain_id],
        )

    def run_references_only(self):
        """Only source reference photos — no AI, no processing."""
        print("[Pipeline] Sourcing reference photos only...")
        refs = self.sourcer.source_terrain_references(self.manifest_path)
        env_refs = self.sourcer.source_environment_references(self.manifest_path)
        print(f"[Pipeline] Sourced {sum(len(v) for v in refs.values())} terrain refs")
        print(f"[Pipeline] Sourced {sum(len(v) for v in env_refs.values())} environment refs")
        return refs, env_refs

    def _find_existing_references(self) -> dict[str, list[Path]]:
        """Find previously downloaded reference images."""
        ref_dir = self.output_dir / "reference_photos"
        result = {}
        if ref_dir.exists():
            for terrain in self.manifest["terrain_types"]:
                tid = terrain["id"]
                refs = sorted(ref_dir.glob(f"ref_{tid}_*"))
                if refs:
                    result[tid] = refs
        return result

    def _find_existing_ai_outputs(self) -> dict[str, list[Path]]:
        """Find previously generated AI images."""
        ai_dir = self.output_dir / "ai_raw"
        result = {}
        if ai_dir.exists():
            for terrain in self.manifest["terrain_types"]:
                tid = terrain["id"]
                tid_dir = ai_dir / tid
                if tid_dir.exists():
                    result[tid] = sorted(tid_dir.glob("*.png"))
        return result

    def _write_report(self, output_dir: Path):
        """Write a pipeline execution report."""
        report_path = output_dir / "pipeline_report.json"
        report = {
            "project": "ShatteredHorizon2032",
            "studio": "Primordia Games",
            "pipeline": "AI Art Generation Pipeline",
            "total_assets_generated": len(self.generated_assets),
            "terrain_types_processed": len(set(
                a["asset_name"].rsplit("_stage", 1)[0]
                for a in self.generated_assets
            )),
            "pbr_maps_per_asset": len(self.manifest["pbr_maps"]),
            "output_format": self.manifest["output_format"],
            "texture_sizes": self.manifest["texture_sizes"],
            "assets": self.generated_assets,
        }
        with open(report_path, "w") as f:
            json.dump(report, f, indent=2, default=str)
        print(f"[Pipeline] Report: {report_path.name}")

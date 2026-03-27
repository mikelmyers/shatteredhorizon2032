"""CLIP embedding comparison for style consistency scoring."""

from __future__ import annotations

import logging
from pathlib import Path
from typing import Any, Optional

logger = logging.getLogger(__name__)

DEFAULT_REFERENCE_DIR = Path(__file__).parent / "bible" / "references"


class CLIPScorer:
    """Score images against a reference set using CLIP cosine similarity.

    If ``open_clip`` (or its dependencies) are not installed the scorer
    operates in *degraded* mode -- ``available`` is ``False`` and
    :meth:`score_image` returns ``0.5`` with a warning.
    """

    def __init__(self, reference_dir: str | Path | None = None) -> None:
        self.reference_dir: Path = Path(reference_dir) if reference_dir else DEFAULT_REFERENCE_DIR
        self.available: bool = False

        # Populated lazily by _load_model.
        self._model: Any = None
        self._preprocess: Any = None
        self._device: str = "cpu"
        self._ref_embeddings: Any = None  # cached mean reference embedding
        self._model_loaded: bool = False

    # ------------------------------------------------------------------
    # Model loading
    # ------------------------------------------------------------------

    def _load_model(self) -> None:
        """Attempt to load ViT-B-32 via ``open_clip``.

        Sets ``self.available = True`` on success, ``False`` on failure.
        """

        if self._model_loaded:
            return

        self._model_loaded = True  # prevent repeated attempts

        try:
            import open_clip  # type: ignore[import-untyped]
            import torch  # type: ignore[import-untyped]

            model, _, preprocess = open_clip.create_model_and_transforms(
                "ViT-B-32", pretrained="laion2b_s34b_b79k"
            )
            device = "cuda" if torch.cuda.is_available() else "cpu"
            model = model.to(device)
            model.eval()

            self._model = model
            self._preprocess = preprocess
            self._device = device
            self.available = True
            logger.info("CLIP model loaded (ViT-B-32, device=%s)", device)
        except ImportError:
            logger.warning(
                "open_clip is not installed -- CLIP scoring disabled. "
                "Install with: pip install open-clip-torch"
            )
            self.available = False
        except Exception as exc:  # noqa: BLE001
            logger.warning("Failed to load CLIP model: %s", exc)
            self.available = False

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def score_image(self, image_path: str) -> float:
        """Return cosine similarity between *image_path* and the mean reference embedding.

        Returns ``0.5`` if the model is unavailable or the reference directory
        is empty, along with a logged warning.
        """

        self._load_model()

        if not self.available:
            logger.warning("CLIP model unavailable -- returning default score 0.5")
            return 0.5

        try:
            import torch  # type: ignore[import-untyped]

            ref_emb = self._get_reference_embeddings()
            if ref_emb is None:
                logger.warning("No reference embeddings available -- returning 0.5")
                return 0.5

            img_emb = self._get_image_embedding(image_path)
            similarity: float = torch.nn.functional.cosine_similarity(img_emb, ref_emb).item()
            return float(similarity)
        except Exception as exc:  # noqa: BLE001
            logger.warning("CLIP scoring failed for %s: %s", image_path, exc)
            return 0.5

    # ------------------------------------------------------------------
    # Embedding helpers
    # ------------------------------------------------------------------

    def _get_reference_embeddings(self) -> Any:
        """Compute and cache the mean embedding for all images in ``reference_dir``."""

        if self._ref_embeddings is not None:
            return self._ref_embeddings

        import torch  # type: ignore[import-untyped]

        if not self.reference_dir.exists():
            return None

        image_exts = {".png", ".jpg", ".jpeg", ".webp", ".bmp", ".tga"}
        ref_images = [
            p for p in self.reference_dir.iterdir() if p.suffix.lower() in image_exts
        ]

        if not ref_images:
            return None

        embeddings: list[Any] = []
        for ref_path in ref_images:
            emb = self._get_image_embedding(str(ref_path))
            embeddings.append(emb)

        stacked = torch.cat(embeddings, dim=0)
        mean_emb = stacked.mean(dim=0, keepdim=True)
        mean_emb = mean_emb / mean_emb.norm(dim=-1, keepdim=True)
        self._ref_embeddings = mean_emb
        return self._ref_embeddings

    def _get_image_embedding(self, image_path: str) -> Any:
        """Compute the CLIP embedding for a single image.

        Returns a normalised tensor of shape ``(1, embed_dim)``.
        """

        import torch  # type: ignore[import-untyped]
        from PIL import Image  # type: ignore[import-untyped]

        img = Image.open(image_path).convert("RGB")
        image_tensor = self._preprocess(img).unsqueeze(0).to(self._device)

        with torch.no_grad():
            embedding = self._model.encode_image(image_tensor)
            embedding = embedding / embedding.norm(dim=-1, keepdim=True)

        return embedding.cpu()

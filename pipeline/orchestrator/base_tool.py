"""Abstract base class for tool handler plugins."""

from abc import ABC, abstractmethod


class ToolHandler(ABC):
    """Base class that all tool plugins must inherit from.

    Each tool handler represents a single external service or local tool
    that can generate assets for the pipeline.
    """

    tool_id: str = ""
    tool_name: str = ""

    @abstractmethod
    def generate(self, request: "AssetRequest", output_dir: str) -> dict:
        """Generate an asset from the given request.

        Args:
            request: The asset generation request.
            output_dir: Directory to write output files into.

        Returns:
            dict with keys:
                success (bool): Whether generation succeeded.
                file_path (str): Path to the generated file, or empty string.
                cost (float): Actual cost in USD for this generation.
                metadata (dict): Any extra metadata from the tool.
                error (str): Error message if success is False.
        """
        pass

    @abstractmethod
    def is_available(self) -> bool:
        """Check whether this tool is currently available.

        Availability means the required API key is set, the local service
        is reachable, or whatever precondition the tool needs.
        """
        pass

    def get_estimated_cost(self, request: "AssetRequest") -> float:
        """Return an estimated cost in USD for the given request.

        Override per tool. Defaults to 0.0 (free).
        """
        return 0.0

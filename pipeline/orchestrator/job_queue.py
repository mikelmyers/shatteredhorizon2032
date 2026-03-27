"""Async job queue for managing concurrent pipeline tasks."""

from __future__ import annotations

import asyncio
import logging
import uuid
from typing import Any, Coroutine

logger = logging.getLogger(__name__)


class JobQueue:
    """Simple async job queue with concurrency limiting via semaphore.

    Jobs are tracked by UUID and can be individually queried or cancelled.
    """

    def __init__(self, max_concurrent: int = 3) -> None:
        self._max_concurrent = max_concurrent
        self._semaphore: asyncio.Semaphore | None = None
        self._jobs: dict[str, dict[str, Any]] = {}
        self._tasks: dict[str, asyncio.Task[Any]] = {}

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _ensure_semaphore(self) -> asyncio.Semaphore:
        """Lazily create the semaphore inside the running event loop."""
        if self._semaphore is None:
            self._semaphore = asyncio.Semaphore(self._max_concurrent)
        return self._semaphore

    async def _run_job(self, job_id: str, coro: Coroutine[Any, Any, Any]) -> Any:
        """Wrap a coroutine with semaphore gating and status tracking."""
        sem = self._ensure_semaphore()
        self._jobs[job_id]["status"] = "waiting"
        try:
            async with sem:
                self._jobs[job_id]["status"] = "running"
                result = await coro
                self._jobs[job_id]["status"] = "completed"
                self._jobs[job_id]["result"] = result
                return result
        except asyncio.CancelledError:
            self._jobs[job_id]["status"] = "cancelled"
            raise
        except Exception as exc:
            self._jobs[job_id]["status"] = "failed"
            self._jobs[job_id]["error"] = str(exc)
            logger.exception("Job %s failed", job_id)
            return None

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def submit(self, coro: Coroutine[Any, Any, Any]) -> str:
        """Submit an async coroutine as a job.

        The coroutine is wrapped in an :class:`asyncio.Task` and will begin
        executing as soon as the semaphore allows.

        Args:
            coro: An awaitable coroutine to run.

        Returns:
            A unique job ID (UUID4 hex string).
        """
        job_id = uuid.uuid4().hex
        self._jobs[job_id] = {
            "id": job_id,
            "status": "pending",
            "result": None,
            "error": "",
        }
        task = asyncio.ensure_future(self._run_job(job_id, coro))
        self._tasks[job_id] = task
        return job_id

    def get_status(self, job_id: str) -> dict[str, Any]:
        """Return the current status dict for a job.

        Returns a dict with keys ``id``, ``status``, ``result``, ``error``.
        If the job_id is unknown, ``status`` will be ``"unknown"``.
        """
        return self._jobs.get(job_id, {
            "id": job_id,
            "status": "unknown",
            "result": None,
            "error": "Job ID not found",
        })

    async def run_batch(
        self,
        jobs: list[Coroutine[Any, Any, Any]],
    ) -> list[dict[str, Any]]:
        """Run a batch of coroutines concurrently, respecting the semaphore.

        Args:
            jobs: List of awaitable coroutines.

        Returns:
            List of status dicts, one per job, in the same order as *jobs*.
        """
        job_ids: list[str] = []
        for coro in jobs:
            job_id = self.submit(coro)
            job_ids.append(job_id)

        # Await all tasks.
        tasks = [self._tasks[jid] for jid in job_ids]
        await asyncio.gather(*tasks, return_exceptions=True)

        return [self.get_status(jid) for jid in job_ids]

    def cancel(self, job_id: str) -> bool:
        """Attempt to cancel a pending or running job.

        Returns:
            ``True`` if the cancellation was issued, ``False`` if the job
            was not found or already finished.
        """
        task = self._tasks.get(job_id)
        if task is None:
            return False
        if task.done():
            return False
        task.cancel()
        return True

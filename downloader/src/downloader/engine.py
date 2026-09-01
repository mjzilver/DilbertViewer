import asyncio
import datetime
import logging
import random

import httpx
import aiosqlite

from .config import Config, FIRST_COMIC, LAST_COMIC, TIMEOUT
from .db import create_tables, load_existing_dates
from .models import ComicTask, Progress
from .processor import ComicProcessor

logger = logging.getLogger(__name__)


class Engine:
    def __init__(self, cfg: Config) -> None:
        self.cfg = cfg

        self.db: aiosqlite.Connection | None = None
        self.session: httpx.AsyncClient | None = None
        self.queue: asyncio.Queue[ComicTask] = asyncio.Queue()

        self.existing_dates: set[str] = set()

        self.progress = Progress()
        self.processor = ComicProcessor(self)

        self._workers: list[asyncio.Task] = []
        self._progress_task: asyncio.Task | None = None

    async def run(self) -> None:
        await self._open_database()

        try:
            await self._prepare_tasks()
            await self._start_session()
            await self._start_workers()

            self._progress_task = asyncio.create_task(self._report_progress())

            await self.queue.join()

            if self._progress_task:
                self._progress_task.cancel()
                try:
                    await self._progress_task
                except asyncio.CancelledError:
                    pass
                self._progress_task = None

            await self._stop_workers()

            self._log_summary()

        finally:
            await self._close_session()
            await self._close_database()

    async def _open_database(self) -> None:
        db_path = self.cfg.base_dir / "metadata.db"

        self.db = await aiosqlite.connect(db_path)

        await create_tables(self.db)
        await self.db.commit()

        self.existing_dates = await load_existing_dates(
            self.db,
            self.cfg.base_dir,
        )

    async def _close_database(self) -> None:
        if self.db is not None:
            await self.db.close()
            self.db = None

    async def _start_session(self) -> None:
        client_kwargs = {
            "timeout": TIMEOUT,
            "limits": httpx.Limits(
                max_connections=self.cfg.concurrency,
                max_keepalive_connections=self.cfg.concurrency,
            ),
        }

        if self.cfg.tor:
            client_kwargs["proxy"] = "socks5://127.0.0.1:9050"

            logger.info("Using Tor SOCKS5 proxy at 127.0.0.1:9050")

        self.session = httpx.AsyncClient(**client_kwargs)

    async def _close_session(self) -> None:
        if self.session is not None:
            await self.session.aclose()
            self.session = None

    async def _prepare_tasks(self) -> None:
        all_dates = [
            FIRST_COMIC + datetime.timedelta(days=i)
            for i in range((LAST_COMIC - FIRST_COMIC).days + 1)
        ]

        to_process = [
            date for date in all_dates if date.isoformat() not in self.existing_dates
        ]

        self.progress.total = len(to_process)

        logger.info(
            "Found %d comics to process out of %d total comics",
            len(to_process),
            len(all_dates),
        )

        for date in to_process:
            await self.queue.put(ComicTask(date=date))

    async def _start_workers(self) -> None:
        self._workers = [
            asyncio.create_task(self._worker(worker_id))
            for worker_id in range(self.cfg.concurrency)
        ]

    async def _stop_workers(self) -> None:
        for _ in self._workers:
            await self.queue.put(None)

        await asyncio.gather(*self._workers)

        self._workers.clear()

    async def _worker(self, worker_id: int) -> None:
        while True:
            self.progress.mark_idle(worker_id)

            task = await self.queue.get()

            if task is None:
                self.queue.task_done()
                self.progress.mark_stopped(worker_id)
                return

            requeued = False

            try:
                self.progress.mark_active(worker_id, task)

                success = await self.processor.process_comic(task)

                if success:
                    self.progress.mark_completed()

                elif task.attempt < self.cfg.max_retries:
                    task.attempt += 1
                    requeued = True

                    self.progress.mark_retrying(worker_id, task)

                    delay = (2**task.attempt) + random.random()

                    logger.warning(
                        "Worker %d: retrying %s in %.2fs " "(attempt %d/%d): %s",
                        worker_id,
                        task.date.isoformat(),
                        delay,
                        task.attempt,
                        self.cfg.max_retries,
                        task.last_error,
                    )

                    await asyncio.sleep(delay)
                    await self.queue.put(task)

                else:
                    self.progress.mark_failed()

                    logger.error(
                        "Worker %d: failed %s after %d attempts: %s",
                        worker_id,
                        task.date.isoformat(),
                        self.cfg.max_retries,
                        task.last_error,
                    )

            except Exception:
                logger.exception(
                    "Worker %d crashed while processing %s",
                    worker_id,
                    task.date.isoformat(),
                )

                self.progress.mark_failed()

            finally:
                self.queue.task_done()

                if not requeued:
                    self.progress.mark_idle(worker_id)

    async def _report_progress(self) -> None:
        try:
            while True:
                await asyncio.sleep(5)
                if self.progress.total > 0:
                    logger.info(
                        "Progress: %.1f%% | Completed: %d | Failed: %d | Active: %d",
                        self.progress.percentage,
                        self.progress.completed,
                        self.progress.failed,
                        self.progress.active,
                    )
        except asyncio.CancelledError:
            pass

    def _log_summary(self) -> None:
        logger.info(
            "Download complete. Total: %d, Completed: %d, Failed: %d",
            self.progress.total,
            self.progress.completed,
            self.progress.failed,
        )

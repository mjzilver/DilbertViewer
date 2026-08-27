import asyncio
import datetime
import logging
import random
import time
from dataclasses import dataclass, field
from pathlib import Path

import aiofiles
from bs4 import BeautifulSoup
from downloader.dl_config import FIRST_COMIC, LAST_COMIC, TIMEOUT, build_config

from .dl_db import create_tables, load_existing_dates, save_comic_with_tags
from .dl_parser import extract_metadata
from .dl_utils import fetch

logger = logging.getLogger(__name__)


@dataclass
class ComicTask:
    date: object
    attempt: int = 0
    last_error: str | None = None
    need_image: bool = True
    need_metadata: bool = True


@dataclass
class WorkerProgress:
    status: str = "idle"
    current_date: str | None = None
    attempt: int = 0


@dataclass
class Progress:
    total: int = 0
    completed: int = 0
    failed: int = 0
    start_time: float = field(default_factory=time.monotonic)

    workers: dict[int, WorkerProgress] = field(default_factory=dict)

    @property
    def finished(self) -> int:
        return self.completed + self.failed

    @property
    def pending(self) -> int:
        return max(0, self.total - self.finished - self.active)

    @property
    def active(self) -> int:
        return sum(
            worker.status in ("active", "retrying") for worker in self.workers.values()
        )

    @property
    def retrying(self) -> int:
        return sum(worker.status == "retrying" for worker in self.workers.values())

    @property
    def percentage(self) -> float:
        if not self.total:
            return 100.0

        return self.finished / self.total * 100

    def worker(self, worker_id: int) -> WorkerProgress:
        return self.workers.setdefault(worker_id, WorkerProgress())

    def mark_active(self, worker_id: int, task: ComicTask) -> None:
        worker = self.worker(worker_id)
        worker.status = "active"
        worker.current_date = task.date.isoformat()
        worker.attempt = task.attempt

    def mark_retrying(self, worker_id: int, task: ComicTask) -> None:
        worker = self.worker(worker_id)
        worker.status = "retrying"
        worker.current_date = task.date.isoformat()
        worker.attempt = task.attempt

    def mark_idle(self, worker_id: int) -> None:
        worker = self.worker(worker_id)
        worker.status = "idle"
        worker.current_date = None
        worker.attempt = 0

    def mark_stopped(self, worker_id: int) -> None:
        worker = self.worker(worker_id)
        worker.status = "stopped"
        worker.current_date = None

    def mark_completed(self) -> None:
        self.completed += 1

    def mark_failed(self) -> None:
        self.failed += 1


class Downloader:
    def __init__(self, cfg):
        self.cfg = cfg

        self.db = None
        self.session = None
        self.queue = asyncio.Queue()

        self.existing_dates: set[str] = set()

        self.progress = Progress()

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

            await self._stop_workers()

            await self._progress_task

            self._log_summary()

        finally:
            await self._close_session()
            await self._close_database()

    async def _open_database(self) -> None:
        import aiosqlite

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
        import httpx

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
            await self.queue.put(ComicTask(date))

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

                success = await self._process_comic(task)

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

    async def _process_comic(self, task: ComicTask) -> bool:
        date = task.date
        date_str = date.isoformat()

        year_folder = self.cfg.base_dir / str(date.year)
        year_folder.mkdir(parents=True, exist_ok=True)

        file_path = year_folder / f"Dilbert_{date_str}.png"

        need_image, need_metadata = await self._needs_work(
            task,
            date_str,
        )

        if not need_image and not need_metadata:
            self.existing_dates.add(date_str)
            return True

        src_url = f"https://dilbert.com/strip/{date_str}"

        try:
            result = await self._fetch_archive_page(src_url)

            if result is None:
                self.existing_dates.add(date_str)
                return True

            html, timestamp = result

        except Exception as e:
            task.last_error = str(e)
            return False

        soup = BeautifulSoup(
            html.decode("utf-8", errors="ignore"),
            "html.parser",
        )

        if need_metadata:
            try:
                await self._extract_and_save_metadata(
                    soup,
                    date_str,
                    file_path,
                )
            except Exception as e:
                task.last_error = f"Metadata error: {e}"
                return False

        if need_image:
            success = await self._handle_image(
                soup,
                timestamp,
                file_path,
                task,
            )

            if not success:
                return False

        await self.db.commit()

        self.existing_dates.add(date_str)

        return True

    async def _needs_work(
        self,
        task: ComicTask,
        date_str: str,
    ) -> tuple[bool, bool]:

        need_image = task.need_image
        need_metadata = task.need_metadata

        async with self.db.execute(
            """
            SELECT
                image_path,
                transcript,
                COALESCE(metadata_checked, 0),
                (
                    SELECT COUNT(*)
                    FROM comic_tags
                    WHERE comic_date = ?
                )
            FROM comics
            WHERE date = ?
            """,
            (date_str, date_str),
        ) as cursor:
            row = await cursor.fetchone()

        if not row:
            return need_image, need_metadata

        image_path_db, transcript_db, metadata_checked, tag_count = row

        if need_image and image_path_db:
            if (self.cfg.base_dir / image_path_db).exists():
                need_image = False

        if need_metadata:
            has_metadata = (
                bool(transcript_db and transcript_db.strip())
                or tag_count > 0
                or metadata_checked > 0
            )

            if has_metadata:
                need_metadata = False

        return need_image, need_metadata

    async def _extract_and_save_metadata(
        self,
        soup,
        date_str,
        image_path,
    ) -> None:

        metadata_div = soup.find(
            "div",
            class_="meta-info-container",
        )

        if not metadata_div:
            return

        transcript, tags = extract_metadata(metadata_div)

        relative_path = Path(str(image_path.relative_to(image_path.parents[1])))

        await save_comic_with_tags(
            self.db,
            date_str,
            relative_path,
            transcript,
            tags,
        )

        logger.info(
            "Saved metadata for %s | " "Transcript: %s | Tags: %s",
            image_path,
            bool(transcript),
            bool(tags),
        )

    async def _fetch_archive_page(self, src_url):
        cdx_url = (
            "https://web.archive.org/cdx/search/cdx?"
            f"url={src_url}"
            "&fl=timestamp"
            "&filter=statuscode:^2"
            "&limit=-1"
        )

        body, status = await fetch(
            self.session,
            cdx_url,
        )

        if body is None:
            raise RuntimeError(f"CDX fetch failed ({status}) - URL: {cdx_url}")

        lines = body.decode("utf-8").splitlines()

        if not lines:
            logger.info(
                "No Wayback capture found for %s",
                src_url,
            )
            return None

        timestamp = lines[-1]

        archived_url = f"https://web.archive.org/web/" f"{timestamp}/{src_url}"

        html, status = await fetch(
            self.session,
            archived_url,
        )

        if html is None:
            raise RuntimeError(
                f"Archive page fetch failed ({status}) " f"- URL: {archived_url}"
            )

        return html, timestamp

    async def _download_image(
        self,
        img_url,
        file_path,
    ) -> None:

        img_data, status = await fetch(
            self.session,
            img_url,
        )

        if not img_data:
            raise RuntimeError(f"Image fetch failed ({status})")

        async with aiofiles.open(
            file_path,
            "wb",
        ) as f:
            await f.write(img_data)

        logger.info(
            "Downloaded image: %s",
            file_path,
        )

    async def _handle_image(
        self,
        soup,
        timestamp,
        file_path,
        task,
    ) -> bool:

        img_tag = soup.find(
            "img",
            class_="img-comic",
        )

        if not img_tag or not img_tag.get("src"):
            logger.warning(
                "No image found for %s",
                file_path,
            )
            return True

        img_src = img_tag["src"]

        img_url = (
            img_src
            if img_src.startswith("https://web.archive.org/")
            else ("https://web.archive.org/web/" f"{timestamp}im_/{img_src}")
        )

        try:
            await self._download_image(
                img_url,
                file_path,
            )
        except Exception as e:
            task.last_error = f"Image download error: {e}"
            return False

        return True

    async def _report_progress(self) -> None:
        last_finished = 0
        last_time = time.monotonic()

        while self.progress.finished < self.progress.total:
            await asyncio.sleep(10)

            now = time.monotonic()

            finished = self.progress.finished

            elapsed = now - last_time
            finished_since_last = finished - last_finished

            current_rate = finished_since_last / elapsed * 60 if elapsed > 0 else 0

            total_elapsed = now - self.progress.start_time

            average_rate = finished / total_elapsed * 60 if total_elapsed > 0 else 0

            logger.info(
                "Progress: %d/%d (%.1f%%) | "
                "Rate: %.1f/min | Avg: %.1f/min | "
                "Pending: %d | Active: %d | "
                "Retrying: %d | Failed: %d",
                finished,
                self.progress.total,
                self.progress.percentage,
                current_rate,
                average_rate,
                self.progress.pending,
                self.progress.active,
                self.progress.retrying,
                self.progress.failed,
            )

            for worker_id, worker in self.progress.workers.items():
                if worker.status in (
                    "active",
                    "retrying",
                ):
                    logger.info(
                        "  Worker %d: %s %s " "(attempt %d)",
                        worker_id,
                        worker.status,
                        worker.current_date,
                        worker.attempt,
                    )

            last_finished = finished
            last_time = now

    def _log_summary(self) -> None:
        elapsed = time.monotonic() - self.progress.start_time

        logger.info(
            "Download finished: " "%d completed, %d failed, " "%d total in %.1fs",
            self.progress.completed,
            self.progress.failed,
            self.progress.total,
            elapsed,
        )


async def start_download():
    cfg = build_config()

    logger.info(cfg)

    downloader = Downloader(cfg)
    await downloader.run()

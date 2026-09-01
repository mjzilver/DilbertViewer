import asyncio
import datetime
import logging
from pathlib import Path
import re

import aiofiles
from bs4 import BeautifulSoup

from .db import save_comic_with_tags
from .parser import extract_metadata
from .utils import fetch
from .models import ComicTask
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .engine import Engine

logger = logging.getLogger(__name__)


class ComicProcessor:
    def __init__(self, engine: "Engine"):
        self.engine = engine
        self.year_cdx_cache: dict[int, dict[str, str]] = {}
        self._cdx_lock = asyncio.Lock()

    async def process_comic(self, task: ComicTask) -> bool:
        date = task.date
        date_str = date.isoformat()

        year_folder = self.engine.cfg.base_dir / str(date.year)
        year_folder.mkdir(parents=True, exist_ok=True)

        file_path = year_folder / f"Dilbert_{date_str}.png"

        need_image, need_metadata = await self._needs_work(task, date_str)

        if not need_image and not need_metadata:
            self.engine.existing_dates.add(date_str)
            return True

        src_url = f"https://dilbert.com/strip/{date_str}"

        try:
            result = await self._fetch_archive_page(src_url, date)

            if result is None:
                self.engine.existing_dates.add(date_str)
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
                await self._extract_and_save_metadata(soup, date_str, file_path)
            except Exception as e:
                task.last_error = f"Metadata error: {e}"
                return False

        if need_image:
            success = await self._handle_image(soup, timestamp, file_path, task)
            if not success:
                return False

        await self.engine.db.commit()
        self.engine.existing_dates.add(date_str)
        return True

    async def _needs_work(self, task: ComicTask, date_str: str) -> tuple[bool, bool]:
        need_image = task.need_image
        need_metadata = task.need_metadata

        async with self.engine.db.execute(
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
            if (self.engine.cfg.base_dir / image_path_db).exists():
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
        self, soup: BeautifulSoup, date_str: str, image_path: Path
    ) -> None:
        metadata_div = soup.find("div", class_="meta-info-container")
        if not metadata_div:
            return

        transcript, tags = extract_metadata(metadata_div)
        relative_path = Path(str(image_path.relative_to(image_path.parents[1])))

        await save_comic_with_tags(
            self.engine.db,
            date_str,
            relative_path,
            transcript,
            tags,
        )

        logger.info(
            "Saved metadata for %s | Transcript: %s | Tags: %s",
            image_path,
            bool(transcript),
            bool(tags),
        )

    async def _fetch_year_cdx(self, year: int) -> dict[str, str]:
        cdx_url = (
            f"https://web.archive.org/cdx/search/cdx?"
            f"url=dilbert.com/strip/{year}-*"
            "&fl=original,timestamp"
            "&filter=statuscode:^2"
            "&collapse=urlkey"
        )
        logger.info("Fetching batch CDX index for year %d...", year)
        body, status = await fetch(self.engine.session, cdx_url)
        if body is None:
            logger.warning(
                "Batch CDX fetch failed for year %d (status: %s), will use single-date fallback",
                year,
                status,
            )
            return {}

        date_to_ts: dict[str, str] = {}
        pattern = re.compile(r"/strip/(\d{4}-\d{2}-\d{2})")
        for line in body.decode("utf-8", errors="ignore").splitlines():
            parts = line.strip().split()
            if len(parts) >= 2:
                orig, ts = parts[0], parts[1]
                m = pattern.search(orig)
                if m:
                    date_to_ts[m.group(1)] = ts

        logger.info(
            "Batch CDX index for year %d loaded %d dates", year, len(date_to_ts)
        )
        return date_to_ts

    async def _get_cached_or_fetch_year_timestamp(
        self, date: datetime.date
    ) -> str | None:
        year = date.year
        date_str = date.isoformat()

        async with self._cdx_lock:
            if year not in self.year_cdx_cache:
                self.year_cdx_cache[year] = await self._fetch_year_cdx(year)

        year_cache = self.year_cdx_cache.get(year, {})
        return year_cache.get(date_str)

    async def _fetch_archive_page(
        self, src_url: str, date: datetime.date
    ) -> tuple[bytes, str] | None:
        batch_ts = await self._get_cached_or_fetch_year_timestamp(date)
        if batch_ts:
            archived_url = f"https://web.archive.org/web/{batch_ts}/{src_url}"
            html, status = await fetch(self.engine.session, archived_url)
            if html is not None:
                return html, batch_ts
            logger.warning(
                "Archive fetch using batch CDX timestamp (%s) failed (%s) for %s; attempting single-date fallback",
                batch_ts,
                status,
                src_url,
            )

        cdx_url = (
            "https://web.archive.org/cdx/search/cdx?"
            f"url={src_url}"
            "&fl=timestamp"
            "&filter=statuscode:^2"
            "&limit=-1"
        )

        body, status = await fetch(self.engine.session, cdx_url)

        if body is None:
            raise RuntimeError(f"CDX fetch failed ({status}) - URL: {cdx_url}")

        lines = body.decode("utf-8").splitlines()
        if not lines:
            logger.info("No Wayback capture found for %s", src_url)
            return None

        timestamp = lines[-1]
        archived_url = f"https://web.archive.org/web/{timestamp}/{src_url}"

        html, status = await fetch(self.engine.session, archived_url)
        if html is None:
            raise RuntimeError(
                f"Archive page fetch failed ({status}) - URL: {archived_url}"
            )

        return html, timestamp

    async def _download_image(self, img_url: str, file_path: Path) -> None:
        img_data, status = await fetch(self.engine.session, img_url)
        if not img_data:
            raise RuntimeError(f"Image fetch failed ({status})")

        async with aiofiles.open(file_path, "wb") as f:
            await f.write(img_data)

        logger.info("Downloaded image: %s", file_path)

    async def _handle_image(
        self, soup: BeautifulSoup, timestamp: str, file_path: Path, task: ComicTask
    ) -> bool:
        img_tag = soup.find("img", class_="img-comic")
        if not img_tag or not img_tag.get("src"):
            logger.warning(f"No image found for {file_path}")
            return True

        img_src = img_tag["src"]

        img_url = (
            img_src
            if img_src.startswith("https://web.archive.org/")
            else f"https://web.archive.org/web/{timestamp}im_/{img_src}"
        )

        try:
            await self._download_image(img_url, file_path)
        except Exception as e:
            task.last_error = f"Image download error: {e}"
            return False

        return True

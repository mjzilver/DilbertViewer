import asyncio
import random

import aiofiles
import logging
from pathlib import Path

from bs4 import BeautifulSoup

from .dl_config import BASE_DIR, FIRST_COMIC
from .dl_utils import fetch
from .dl_parser import extract_metadata
from .dl_db import save_comic_with_tags

logger = logging.getLogger(__name__)


class ComicTask:
    def __init__(self, date):
        self.date = date
        self.attempt = 0
        self.last_error = None


async def process_comic(session, db, task, existing_dates):
    date = task.date
    date_str = date.isoformat()
    year_folder = BASE_DIR / str(date.year)
    year_folder.mkdir(parents=True, exist_ok=True)
    file_path = year_folder / f"Dilbert_{date_str}.png"

    # Consider metadata present if transcript/tags exist or if metadata was checked previously
    metadata_exists = False
    async with db.execute(
        "SELECT image_path, transcript, COALESCE(metadata_checked, 0) FROM comics WHERE date=?",
        (date_str,),
    ) as cursor:
        row = await cursor.fetchone()
        if row:
            image_path_db, transcript_db, metadata_checked = row
            tag_count = 0
            async with db.execute(
                "SELECT COUNT(*) FROM comic_tags WHERE comic_date=?", (date_str,)
            ) as c:
                r = await c.fetchone()
                tag_count = r[0] if r else 0

            image_exists = False
            if image_path_db:
                try:
                    image_exists = (BASE_DIR / image_path_db).exists()
                except Exception:
                    image_exists = False

            metadata_exists = (
                (transcript_db and transcript_db.strip())
                or (tag_count > 0)
                or (metadata_checked and metadata_checked > 0)
            )

    if file_path.exists() and metadata_exists:
        existing_dates.add(date_str)
        return True

    src_url = f"https://dilbert.com/strip/{date_str}"
    cdx_url = f"https://web.archive.org/cdx/search/cdx?url={src_url}&fl=timestamp&filter=statuscode:^2&limit=-1"
    cdx_body, status = await fetch(session, cdx_url)
    if not cdx_body:
        if status == 429:
            task.last_error = f"HTTP 429 from {cdx_url}"
            return False
        task.last_error = f"Failed to fetch CDX {cdx_url}: status={status}"
        return False

    lines = cdx_body.decode("utf-8").splitlines()
    if not lines:
        return True

    timestamp = lines[-1]
    archived_page_url = f"https://web.archive.org/web/{timestamp}/{src_url}"
    html, status = await fetch(session, archived_page_url)
    if not html:
        if status == 429:
            task.last_error = f"HTTP 429 from {archived_page_url}"
            return False
        task.last_error = f"Failed to fetch archived page {archived_page_url}: status={status}"
        return False

    soup = BeautifulSoup(html.decode("utf-8", errors="ignore"), "html.parser")
    metadata_div = soup.find("div", class_="meta-info-container")
    transcript, tags = "", []
    if metadata_div:
        transcript, tags = extract_metadata(metadata_div)
        relative_path = Path(str(date.year)) / f"Dilbert_{date_str}.png"
        await save_comic_with_tags(db, date_str, relative_path, transcript, tags)
        logger.info(
            f"Saved metadata/checked for image: {file_path} - Transcript found: {bool(transcript)}, Tags found: {bool(tags)}"
        )

    if not file_path.exists():
        img_tag = soup.find("img", class_="img-comic")
        if img_tag and img_tag.get("src"):
            img_src = img_tag["src"]
            img_url = (
                img_src
                if img_src.startswith("https://web.archive.org/")
                else f"https://web.archive.org/web/{timestamp}im_/{img_src}"
            )
            img_data, status = await fetch(session, img_url)
            if img_data:
                try:
                    async with aiofiles.open(file_path, "wb") as f:
                        await f.write(img_data)
                    logger.info(f"Downloaded image: {file_path}")
                except Exception as e:
                    logger.error(f"Failed to save image {file_path}: {e}")
                    task.last_error = f"Failed to save image {file_path}: {e}"
                    return False
            else:
                if status == 429:
                    task.last_error = f"HTTP 429 when fetching image {img_url}"
                    return False

    existing_dates.add(date_str)
    return True


async def worker(
    worker_id, session, db, queue, pbar, existing_dates, BATCH_COMMIT, MAX_RETRIES
):
    processed_since_commit = 0
    while True:
        task = await queue.get()
        if task is None:
            queue.task_done()
            break
        try:
            success = await process_comic(session, db, task, existing_dates)
            if not success and task.attempt < MAX_RETRIES:
                task.attempt += 1
                await asyncio.sleep((2**task.attempt) + random.random())
                await queue.put(task)
            elif not success:
                logger.error(
                    f"Failed to download {task.date.isoformat()} after {MAX_RETRIES} attempts: {task.last_error}"
                )
                pbar.update(1)
            else:
                processed_since_commit += 1
                pbar.update(1)
                if processed_since_commit >= BATCH_COMMIT:
                    await db.commit()
                    processed_since_commit = 0
        except Exception as e:
            logger.error(f"Worker {worker_id} error processing {task.date}: {e}")
        finally:
            queue.task_done()
    if processed_since_commit > 0:
        await db.commit()

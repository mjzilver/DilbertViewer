import asyncio
import random
import logging
from pathlib import Path

import aiofiles
from bs4 import BeautifulSoup

from .dl_utils import fetch
from .dl_parser import extract_metadata
from .dl_db import save_comic_with_tags

logger = logging.getLogger(__name__)


class ComicTask:
    def __init__(self, date, need_image=True, need_metadata=True):
        self.date = date
        self.attempt = 0
        self.last_error = None
        self.need_image = need_image
        self.need_metadata = need_metadata


async def _fetch_archive_page(session, src_url):
    cdx_url = (
        "https://web.archive.org/cdx/search/cdx?"
        f"url={src_url}&fl=timestamp&filter=statuscode:^2&limit=-1"
    )

    body, status = await fetch(session, cdx_url)
    if not body:
        raise RuntimeError(f"CDX fetch failed ({status})")

    lines = body.decode("utf-8").splitlines()
    if not lines:
        return None

    timestamp = lines[-1]
    archived_url = f"https://web.archive.org/web/{timestamp}/{src_url}"

    html, status = await fetch(session, archived_url)
    if not html:
        raise RuntimeError(f"Archive page fetch failed ({status})")

    return html, timestamp, archived_url


async def _download_image(session, img_url, file_path):
    img_data, status = await fetch(session, img_url)
    if not img_data:
        raise RuntimeError(f"Image fetch failed ({status})")

    async with aiofiles.open(file_path, "wb") as f:
        await f.write(img_data)

    logger.info(f"Downloaded image: {file_path}")


async def _extract_and_save_metadata(db, soup, date_str, image_path):
    metadata_div = soup.find("div", class_="meta-info-container")
    if not metadata_div:
        return

    transcript, tags = extract_metadata(metadata_div)

    relative_path = Path(str(image_path.relative_to(image_path.parents[1])))
    await save_comic_with_tags(db, date_str, relative_path, transcript, tags)

    logger.info(
        f"Saved metadata for {image_path} | "
        f"Transcript: {bool(transcript)} | Tags: {bool(tags)}"
    )


async def _needs_work(db, task, date_str, base_dir):
    need_image = task.need_image
    need_metadata = task.need_metadata

    image_path_db = None
    transcript_db = None
    metadata_checked = 0

    async with db.execute(
        "SELECT image_path, transcript, COALESCE(metadata_checked,0) "
        "FROM comics WHERE date=?",
        (date_str,),
    ) as cursor:
        row = await cursor.fetchone()
        if row:
            image_path_db, transcript_db, metadata_checked = row

    if need_image and image_path_db:
        if (base_dir / image_path_db).exists():
            need_image = False

    if need_metadata:
        async with db.execute(
            "SELECT COUNT(*) FROM comic_tags WHERE comic_date=?",
            (date_str,),
        ) as c:
            r = await c.fetchone()
            tag_count = r[0] if r else 0

        has_metadata = (
            bool(transcript_db and transcript_db.strip())
            or tag_count > 0
            or metadata_checked > 0
        )

        if has_metadata:
            need_metadata = False

    return need_image, need_metadata


async def _handle_image(session, soup, timestamp, file_path, task):
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
        await _download_image(session, img_url, file_path)
    except Exception as e:
        task.last_error = f"Image download error: {e}"
        return False

    return True


async def process_comic(session, db, task, existing_dates, base_dir):
    date = task.date
    date_str = date.isoformat()

    year_folder = base_dir / str(date.year)
    year_folder.mkdir(parents=True, exist_ok=True)

    file_path = year_folder / f"Dilbert_{date_str}.png"

    need_image, need_metadata = await _needs_work(db, task, date_str, base_dir)

    if not need_image and not need_metadata:
        existing_dates.add(date_str)
        return True

    src_url = f"https://dilbert.com/strip/{date_str}"

    try:
        result = await _fetch_archive_page(session, src_url)
        if not result:
            existing_dates.add(date_str)
            return True

        html, timestamp, archived_page_url = result

    except Exception as e:
        task.last_error = str(e)
        return False

    soup = BeautifulSoup(
        html.decode("utf-8", errors="ignore"),
        "html.parser",
    )

    if need_metadata:
        try:
            await _extract_and_save_metadata(db, soup, date_str, file_path)
        except Exception as e:
            task.last_error = f"Metadata error: {e}"
            return False

    if need_image:
        success = await _handle_image(session, soup, timestamp, file_path, task)
        if not success:
            return False

    existing_dates.add(date_str)
    return True


async def worker(
    worker_id,
    session,
    db,
    queue,
    pbar,
    existing_dates,
    base_dir,
    BATCH_COMMIT,
    MAX_RETRIES,
):
    processed_since_commit = 0

    while True:
        task = await queue.get()

        if task is None:
            queue.task_done()
            break

        try:
            success = await process_comic(
                session,
                db,
                task,
                existing_dates,
                base_dir,
            )

            if not success and task.attempt < MAX_RETRIES:
                task.attempt += 1
                await asyncio.sleep((2**task.attempt) + random.random())
                await queue.put(task)

            elif not success:
                logger.error(
                    f"Failed {task.date.isoformat()} "
                    f"after {MAX_RETRIES} attempts: {task.last_error}"
                )
                pbar.update(1)

            else:
                processed_since_commit += 1
                pbar.update(1)

                if processed_since_commit >= BATCH_COMMIT:
                    await db.commit()
                    processed_since_commit = 0

        except Exception as e:
            logger.error(f"Worker {worker_id} crash: {e}")

        finally:
            queue.task_done()

    if processed_since_commit > 0:
        await db.commit()

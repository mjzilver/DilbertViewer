import asyncio
import httpx
from pathlib import Path
import re
import datetime
from tqdm.asyncio import tqdm_asyncio
import logging

FIRST_COMIC = datetime.date(1989, 4, 16)
LAST_COMIC = datetime.date(2023, 3, 12)
BASE_DIR = Path("./Dilbert")
CONCURRENCY = 8
LOG_FILE = "dilbert_downloader.log"

IMG_RE = re.compile(
    r'<img[^>]*class=["\'][^"\']*img-comic[^"\']*["\'][^>]*\bsrc=["\']([^"\']+)["\']',
    re.I,
)

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/117.0.0.0 Safari/537.36",
    "Accept": "image/webp,image/apng,image/*,*/*;q=0.8",
    "Referer": "https://dilbert.com/",
}

BASE_DIR.mkdir(parents=True, exist_ok=True)

logging.basicConfig(
    filename=LOG_FILE,
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)
logger = logging.getLogger(__name__)
logging.getLogger("httpx").setLevel(logging.WARNING)

import random


async def fetch(session, url, retries=5, base_backoff=5):
    for attempt in range(retries):
        try:
            resp = await session.get(
                url, headers=HEADERS, timeout=30, follow_redirects=True
            )
            if resp.status_code != 200:
                logger.warning(f"Non-200 status code {resp.status_code} for URL: {url}")
                return None, resp.status_code
            return resp.content, resp.status_code
        except Exception as e:
            wait_time = base_backoff * (2**attempt) + random.uniform(1, 3)
            logger.warning(
                f"Attempt {attempt + 1}/{retries} failed for URL {url}: {e}. "
                f"Retrying in {wait_time:.2f} seconds..."
            )
            await asyncio.sleep(wait_time)

    logger.error(f"All {retries} attempts failed for URL {url}")
    return None, None


async def fetch_comic(session, date: datetime.date, semaphore: asyncio.Semaphore):
    date_str = date.isoformat()
    year_folder = BASE_DIR / str(date.year)
    year_folder.mkdir(parents=True, exist_ok=True)
    file_path = year_folder / f"Dilbert_{date_str}.png"

    if file_path.exists():
        return

    src_url = f"https://dilbert.com/strip/{date_str}"
    cdx_url = f"https://web.archive.org/cdx/search/cdx?url={src_url}&fl=timestamp&filter=statuscode:^2&limit=-1"

    async with semaphore:
        cdx_body, status = await fetch(session, cdx_url)
        if not cdx_body:
            logger.warning(f"Failed to fetch CDX for {date_str} (status {status})")
            return

        lines = cdx_body.decode("utf-8").splitlines()
        if not lines:
            logger.warning(f"No archive entries found for {date_str}")
            return

        timestamp = lines[-1]
        archived_page_url = f"https://web.archive.org/web/{timestamp}/{src_url}"

        html, status = await fetch(session, archived_page_url)
        if not html:
            logger.warning(
                f"Failed to fetch archived page for {date_str} (status {status})"
            )
            return

        match = IMG_RE.search(html.decode("utf-8", errors="ignore"))
        if not match:
            logger.warning(f"No comic image found for {date_str}")
            return

        img_src = match.group(1)
        img_url = (
            img_src
            if img_src.startswith("https://web.archive.org/")
            else f"https://web.archive.org/web/{timestamp}im_/{img_src}"
        )

        img_data, status = await fetch(session, img_url)
        if not img_data:
            logger.warning(f"Failed to download image for {date_str} (status {status})")
            return

        try:
            file_path.write_bytes(img_data)
            logger.info(f"Successfully downloaded: {file_path}")
        except Exception as e:
            logger.error(f"Failed to save image {file_path}: {e}")


async def main():
    all_dates = [
        FIRST_COMIC + datetime.timedelta(days=i)
        for i in range((LAST_COMIC - FIRST_COMIC).days + 1)
    ]
    semaphore = asyncio.Semaphore(CONCURRENCY)
    async with httpx.AsyncClient() as session:
        tasks = [fetch_comic(session, d, semaphore) for d in all_dates]
        await tqdm_asyncio.gather(*tasks, desc="Downloading comics", total=len(tasks))


if __name__ == "__main__":
    logger.info("Starting Dilbert downloader")
    asyncio.run(main())
    logger.info("Downloader finished")

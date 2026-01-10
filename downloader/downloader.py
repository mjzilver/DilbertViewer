import asyncio
import httpx
from pathlib import Path
import datetime
from tqdm.asyncio import tqdm_asyncio
import logging
from bs4 import BeautifulSoup
import aiosqlite
import json
import random

FIRST_COMIC = datetime.date(1989, 4, 16)
LAST_COMIC = datetime.date(2023, 3, 12)
BASE_DIR = Path("../Dilbert")
CONCURRENCY = 3
LOG_FILE = "dilbert_downloader.log"

HEADERS = {
    "User-Agent": "Mozilla/5.0",
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

TIMEOUT = httpx.Timeout(
    connect=10.0,
    read=100.0, # Web archive can be super slow
    write=10.0,
    pool=10.0,
)


async def fetch(
    session: httpx.AsyncClient,
    url: str,
    retries: int = 3,
    base_backoff: int = 5,
):
    request_id = random.randint(100000, 999999)

    for attempt in range(1, retries + 1):
        try:
            resp = await session.get(
                url,
                headers=HEADERS,
                timeout=TIMEOUT,
                follow_redirects=True,
            )

            if resp.status_code == 429:
                wait = 10 * 60
                logger.warning(
                    "[%s] 429 Too Many Requests: %s (sleep %ds)",
                    request_id,
                    url,
                    wait,
                )
                await asyncio.sleep(wait)
                continue

            if resp.status_code != 200:
                logger.warning(
                    "[%s] HTTP %d for %s (attempt %d/%d)",
                    request_id,
                    resp.status_code,
                    url,
                    attempt,
                    retries,
                )
                return None, resp.status_code

            return resp.content, resp.status_code

        except httpx.HTTPError as e:
            logger.warning(
                "[%s] %s for %s (attempt %d/%d)",
                request_id,
                type(e).__name__,
                url,
                attempt,
                retries,
            )

        except Exception as e:
            logger.error(
                "[%s] Unexpected %s for %s: %s",
                request_id,
                type(e).__name__,
                url,
                e,
            )
            break

        if attempt < retries:
            wait = base_backoff * (2 ** (attempt - 1)) + random.uniform(1, 3)
            await asyncio.sleep(wait)

    logger.error("[%s] All %d attempts failed for %s", request_id, retries, url)
    return None, None


def extract_metadata(metadata_div):
    tags = []
    tags_p = metadata_div.find("p", class_="small comic-tags")
    if tags_p:
        for a in tags_p.find_all("a"):
            tag_text = a.get_text(strip=True)
            if tag_text.startswith("#"):
                tag_text = tag_text[1:]
            tags.append(tag_text)

    transcript = ""
    transcript_div = metadata_div.find("div", class_="comic-transcript")
    if transcript_div:
        p = transcript_div.find("p")
        if p:
            transcript = p.get_text(strip=True)
    return transcript, tags


async def save_comic_with_tags(db, date_str, relative_path, transcript, tags):
    await db.execute(
        "INSERT OR REPLACE INTO comics (date, image_path, transcript) VALUES (?, ?, ?)",
        (date_str, str(relative_path), transcript),
    )

    for tag in tags:
        await db.execute("INSERT OR IGNORE INTO tags (name) VALUES (?)", (tag,))
        async with db.execute("SELECT id FROM tags WHERE name=?", (tag,)) as cursor:
            row = await cursor.fetchone()
            tag_id = row[0]

        await db.execute(
            "INSERT OR IGNORE INTO comic_tags (comic_date, tag_id) VALUES (?, ?)",
            (date_str, tag_id),
        )

    await db.commit()


async def fetch_comic(session, db, date: datetime.date, semaphore: asyncio.Semaphore):
    date_str = date.isoformat()
    year_folder = BASE_DIR / str(date.year)
    year_folder.mkdir(parents=True, exist_ok=True)
    file_path = year_folder / f"Dilbert_{date_str}.png"

    async with db.execute(
        "SELECT COUNT(*) FROM comics WHERE date=?", (date_str,)
    ) as cursor:
        row = await cursor.fetchone()
        metadata_exists = row[0] > 0

    if file_path.exists() and metadata_exists:
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
            logger.warning(f"No archive entries for {date_str}")
            return

        timestamp = lines[-1]
        archived_page_url = f"https://web.archive.org/web/{timestamp}/{src_url}"

        html, status = await fetch(session, archived_page_url)
        if not html:
            logger.warning(
                f"Failed to fetch archived page for {date_str} (status {status})"
            )
            return

        soup = BeautifulSoup(html.decode("utf-8", errors="ignore"), "html.parser")

        metadata_div = soup.find("div", class_="meta-info-container")
        transcript, tags = "", []
        if metadata_div:
            transcript, tags = extract_metadata(metadata_div)

            if not transcript:
                logger.warning(f"[{archived_page_url}] Transcript not found")
            if not tags:
                logger.warning(f"[{archived_page_url}] Tags not found")

            tags_str = json.dumps(tags)

            relative_path = Path(str(date.year)) / f"Dilbert_{date_str}.png"

            await save_comic_with_tags(db, date_str, relative_path, transcript, tags)

        if not file_path.exists():
            img_tag = soup.find("img", class_="img-comic")
            if not img_tag or not img_tag.get("src"):
                logger.warning(f"No comic image found for {date_str}")
                return

            img_src = img_tag["src"]
            img_url = (
                img_src
                if img_src.startswith("https://web.archive.org/")
                else f"https://web.archive.org/web/{timestamp}im_/{img_src}"
            )
            img_data, status = await fetch(session, img_url)
            if not img_data:
                logger.warning(
                    f"Failed to download image for {date_str} (status {status})"
                )
                return

            try:
                file_path.write_bytes(img_data)
                logger.info(f"Downloaded image: {file_path}")
            except Exception as e:
                logger.error(f"Failed to save image {file_path}: {e}")


async def main():
    async with aiosqlite.connect(BASE_DIR / "metadata.db") as db:
        await db.execute(
            """
            CREATE TABLE IF NOT EXISTS comics (
                date TEXT PRIMARY KEY,
                image_path TEXT,
                transcript TEXT,
                tags TEXT
            )
        """
        )

        await db.execute(
            """
            CREATE TABLE IF NOT EXISTS tags (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE
            )
        """
        )

        await db.execute(
            """
            CREATE TABLE IF NOT EXISTS comic_tags (
                comic_date TEXT,
                tag_id INTEGER,
                PRIMARY KEY (comic_date, tag_id),
                FOREIGN KEY (comic_date) REFERENCES comics(date),
                FOREIGN KEY (tag_id) REFERENCES tags(id)
            )
        """
        )
        await db.commit()

        all_dates = [
            FIRST_COMIC + datetime.timedelta(days=i)
            for i in range((LAST_COMIC - FIRST_COMIC).days + 1)
        ]
        semaphore = asyncio.Semaphore(CONCURRENCY)

        async with httpx.AsyncClient() as session:
            tasks = [fetch_comic(session, db, d, semaphore) for d in all_dates]
            await tqdm_asyncio.gather(
                *tasks, desc="Downloading comics", total=len(tasks)
            )


if __name__ == "__main__":
    logger.info("Starting Dilbert downloader")
    asyncio.run(main())
    logger.info("Downloader finished")

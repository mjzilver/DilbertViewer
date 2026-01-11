import asyncio
import httpx
from pathlib import Path
import datetime
from tqdm.asyncio import tqdm
import logging
from bs4 import BeautifulSoup
import aiosqlite
import random
import aiofiles

FIRST_COMIC = datetime.date(1989, 4, 16)
LAST_COMIC = datetime.date(2023, 3, 12)
BASE_DIR = Path("../Dilbert")
CONCURRENCY = 20
LOG_FILE = "dilbert_downloader.log"
MAX_RETRIES = 3
BATCH_COMMIT = 50

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
    read=100.0,
    write=10.0,
    pool=10.0,
)


class ComicTask:
    def __init__(self, date):
        self.date = date
        self.attempt = 0


async def fetch(session, url):
    try:
        resp = await session.get(
            url, headers=HEADERS, timeout=TIMEOUT, follow_redirects=True
        )
        if resp.status_code == 429:
            await asyncio.sleep(10 * 60)
            return None, 429
        if resp.status_code != 200:
            return None, resp.status_code
        return resp.content, resp.status_code
    except httpx.HTTPError:
        return None, None
    except Exception as e:
        logger.error("Unexpected %s for %s: %s", type(e).__name__, url, e)
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


async def process_comic(session, db, task, existing_dates):
    date = task.date
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
        existing_dates.add(date_str)
        return True

    src_url = f"https://dilbert.com/strip/{date_str}"
    cdx_url = f"https://web.archive.org/cdx/search/cdx?url={src_url}&fl=timestamp&filter=statuscode:^2&limit=-1"
    cdx_body, status = await fetch(session, cdx_url)
    if not cdx_body:
        if status == 429:
            return False
        return False

    lines = cdx_body.decode("utf-8").splitlines()
    if not lines:
        return True

    timestamp = lines[-1]
    archived_page_url = f"https://web.archive.org/web/{timestamp}/{src_url}"
    html, status = await fetch(session, archived_page_url)
    if not html:
        if status == 429:
            return False
        return False

    soup = BeautifulSoup(html.decode("utf-8", errors="ignore"), "html.parser")
    metadata_div = soup.find("div", class_="meta-info-container")
    transcript, tags = "", []
    if metadata_div:
        transcript, tags = extract_metadata(metadata_div)
        relative_path = Path(str(date.year)) / f"Dilbert_{date_str}.png"
        await save_comic_with_tags(db, date_str, relative_path, transcript, tags)
        logger.info(f"Saved metadata for image: {file_path}")

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
                    return False
            else:
                if status == 429:
                    return False

    existing_dates.add(date_str)
    return True


async def worker(worker_id, session, db, queue, pbar, existing_dates):
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
                    f"Failed to download {task.date.isoformat()} after {MAX_RETRIES} attempts"
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


async def main():
    async with aiosqlite.connect(BASE_DIR / "metadata.db") as db:
        await db.execute(
            """
            CREATE TABLE IF NOT EXISTS comics (
                date TEXT PRIMARY KEY,
                image_path TEXT,
                transcript TEXT
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

        existing_dates = set()
        async with db.execute("SELECT date FROM comics") as cursor:
            async for row in cursor:
                existing_dates.add(row[0])

        all_dates = [
            FIRST_COMIC + datetime.timedelta(days=i)
            for i in range((LAST_COMIC - FIRST_COMIC).days + 1)
        ]
        queue = asyncio.Queue()
        for date in all_dates:
            await queue.put(ComicTask(date))

        pbar = tqdm(total=len(all_dates), desc="Downloading comics")
        async with httpx.AsyncClient(
            limits=httpx.Limits(max_connections=CONCURRENCY * 2)
        ) as session:
            workers = [
                asyncio.create_task(worker(i, session, db, queue, pbar, existing_dates))
                for i in range(CONCURRENCY)
            ]
            await queue.join()
            for _ in range(CONCURRENCY):
                await queue.put(None)
            await asyncio.gather(*workers)
        pbar.close()


if __name__ == "__main__":
    logger.info("Starting Dilbert downloader")
    asyncio.run(main())
    logger.info("Downloader finished")

import datetime
from pathlib import Path
import httpx

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

TIMEOUT = httpx.Timeout(
    connect=10.0,
    read=100.0,
    write=10.0,
    pool=10.0,
)

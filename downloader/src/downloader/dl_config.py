from dataclasses import dataclass
import datetime
from pathlib import Path
import httpx
import argparse

FIRST_COMIC = datetime.date(1989, 4, 16)
LAST_COMIC = datetime.date(2023, 3, 12)


@dataclass
class Config:
    base_dir: Path = Path("../Dilbert")
    concurrency: int = 20
    max_retries: int = 3
    tor: bool = False


def build_config(argv=None):
    parser = argparse.ArgumentParser(description="Dilbert downloader configuration")
    parser.add_argument(
        "--base-dir",
        type=Path,
        default=Path("../Dilbert"),
        help="Base directory for downloads",
    )
    parser.add_argument(
        "--concurrency", type=int, default=20, help="Number of concurrent downloads"
    )
    parser.add_argument(
        "--max-retries", type=int, default=3, help="Maximum number of retries"
    )
    parser.add_argument(
        "--tor",
        action="store_true",
        help="Route HTTP requests through Tor SOCKS5 at 127.0.0.1:9050",
    )

    args = parser.parse_args(argv)

    cfg = Config(
        base_dir=args.base_dir,
        concurrency=args.concurrency,
        max_retries=args.max_retries,
        tor=args.tor,
    )

    cfg.base_dir.mkdir(parents=True, exist_ok=True)
    return cfg


HEADERS = {
    "User-Agent": "Mozilla/5.0",
    "Accept": "image/webp,image/apng,image/*,*/*;q=0.8",
    "Referer": "https://dilbert.com/",
}

TIMEOUT = httpx.Timeout(
    connect=60.0,
    read=100.0,
    write=10.0,
    pool=10.0,
)

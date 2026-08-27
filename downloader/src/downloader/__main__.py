import asyncio
import logging

from .dl_config import build_config
from .dl_tasks import Downloader

logger = logging.getLogger(__name__)


def configure_logging():
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s | %(levelname)s | %(name)s | %(message)s",
        handlers=[
            logging.FileHandler(
                "dilbert_downloader.log",
                encoding="utf-8",
            ),
            logging.StreamHandler(),
        ],
    )


async def start_download():
    cfg = build_config()

    logger.info(cfg)

    downloader = Downloader(cfg)
    await downloader.run()


def main():
    configure_logging()

    try:
        asyncio.run(start_download())
    except KeyboardInterrupt:
        logger.info("Download interrupted by user")


if __name__ == "__main__":
    main()

import asyncio
import logging

logger = logging.getLogger(__name__)


from downloader.dl_main import start_download

logging.basicConfig(
    filename="dilbert_downloader.log",
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)
logging.getLogger("httpx").setLevel(logging.WARNING)


def main():
    logger.info("Starting Dilbert downloader")
    asyncio.run(start_download())
    logger.info("Downloader finished")


if __name__ == "__main__":
    main()

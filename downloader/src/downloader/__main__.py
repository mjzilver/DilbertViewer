import asyncio
import logging
import multiprocessing

multiprocessing.set_start_method("spawn", force=True)

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
    try:
        asyncio.run(start_download())
    except KeyboardInterrupt:
        logger.info("Received keyboard interrupt")
        pass
    logger.info("Downloader finished")


if __name__ == "__main__":
    main()

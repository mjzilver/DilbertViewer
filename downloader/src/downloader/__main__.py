import asyncio
import cProfile
import logging
import multiprocessing
import pstats
import sys

from .dl_main import start_download

logging.basicConfig(
    filename="dilbert_downloader.log",
    level=logging.DEBUG,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    force=True,
)

logging.getLogger("httpx").setLevel(logging.WARNING)
logging.getLogger("asyncio").setLevel(logging.WARNING)
logging.getLogger("aiosqlite").setLevel(logging.WARNING)
logging.getLogger("httpcore").setLevel(logging.WARNING)

logger = logging.getLogger(__name__)


def main():
    multiprocessing.set_start_method("spawn", force=True)

    logger.info("Starting Dilbert downloader")

    if "--profile" in sys.argv:
        sys.argv.remove("--profile")

        logger.info("Running with profiling enabled")

        cProfile.run(
            "asyncio.run(start_download())",
            "profile_stats.prof",
        )

        stats = pstats.Stats("profile_stats.prof")
        stats.strip_dirs().sort_stats("cumulative").print_stats(20)

    else:
        try:
            asyncio.run(start_download())
        except KeyboardInterrupt:
            logger.info("Received keyboard interrupt")

    logger.info("Downloader finished")


if __name__ == "__main__":
    main()

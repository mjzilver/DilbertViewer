import asyncio
import datetime
import logging

import aiosqlite
import httpx
from tqdm.asyncio import tqdm

from .dl_config import (
    build_config,
    FIRST_COMIC,
    LAST_COMIC,
)
from .dl_db import create_tables, load_existing_dates
from .dl_tasks import ComicTask, worker

logger = logging.getLogger(__name__)


async def start_download():
    cfg = build_config()

    async with aiosqlite.connect(cfg.base_dir / "metadata.db") as db:
        await create_tables(db)
        await db.commit()

        existing_dates = await load_existing_dates(
            db,
            cfg.base_dir,
        )

        all_dates = [
            FIRST_COMIC + datetime.timedelta(days=i)
            for i in range((LAST_COMIC - FIRST_COMIC).days + 1)
        ]

        to_process = []

        for date in all_dates:
            date_str = date.isoformat()

            if date_str in existing_dates:
                continue

            to_process.append(date)

        logger.info(
            "Found %d comics to process out of %d total comics",
            len(to_process),
            len(all_dates),
        )

        queue = asyncio.Queue()

        for date in to_process:
            await queue.put(ComicTask(date))

        async with httpx.AsyncClient(
            limits=httpx.Limits(
                max_connections=cfg.concurrency,
                max_keepalive_connections=cfg.concurrency,
            ),
        ) as session:
            db_commit_lock = asyncio.Lock()

            workers = []

            for worker_id in range(cfg.concurrency):
                task = asyncio.create_task(
                    worker(
                        worker_id,
                        session,
                        db,
                        queue,
                        existing_dates,
                        cfg.base_dir,
                        cfg.batch_commit,
                        cfg.max_retries,
                        db_commit_lock,
                    )
                )

                workers.append(task)

            await queue.join()

            for _ in range(cfg.concurrency):
                await queue.put(None)

            await asyncio.gather(*workers)

        logger.info("Downloader finished")

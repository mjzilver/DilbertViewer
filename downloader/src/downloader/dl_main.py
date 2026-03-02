import asyncio
import datetime
from tqdm.asyncio import tqdm
import aiosqlite
import httpx

from .dl_config import (
    build_config,
    FIRST_COMIC,
    LAST_COMIC,
)
from .dl_db import create_tables, load_existing_dates
from .dl_tasks import ComicTask, worker


async def start_download():
    cfg = build_config()
    async with aiosqlite.connect(cfg.base_dir / "metadata.db") as db:
        await create_tables(db)
        await db.commit()

        existing_dates = await load_existing_dates(db, cfg.base_dir)

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

        queue = asyncio.Queue()
        for date in to_process:
            await queue.put(ComicTask(date))

        pbar = tqdm(total=len(to_process), desc="Downloading comics")
        async with httpx.AsyncClient(
            limits=httpx.Limits(max_connections=cfg.concurrency)
        ) as session:
            workers = [
                asyncio.create_task(
                    worker(
                        i,
                        session,
                        db,
                        queue,
                        pbar,
                        existing_dates,
                        cfg.base_dir,
                        cfg.batch_commit,
                        cfg.max_retries,
                    )
                )
                for i in range(cfg.concurrency)
            ]
            await queue.join()
            for _ in range(cfg.concurrency):
                await queue.put(None)
            await asyncio.gather(*workers)
        pbar.close()

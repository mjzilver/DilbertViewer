import logging
import asyncio
import httpx
from downloader.dl_config import HEADERS, TIMEOUT

logger = logging.getLogger(__name__)


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

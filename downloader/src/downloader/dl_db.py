import logging
from pathlib import Path

logger = logging.getLogger(__name__)


async def create_tables(db):
    await db.execute("""
        CREATE TABLE IF NOT EXISTS comics (
            date TEXT PRIMARY KEY,
            image_path TEXT,
            transcript TEXT,
            metadata_checked INTEGER DEFAULT 0
        )
        """)
    await db.execute("""
        CREATE TABLE IF NOT EXISTS tags (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE
        )
        """)
    await db.execute("""
        CREATE TABLE IF NOT EXISTS comic_tags (
            comic_date TEXT,
            tag_id INTEGER,
            PRIMARY KEY (comic_date, tag_id),
            FOREIGN KEY (comic_date) REFERENCES comics(date),
            FOREIGN KEY (tag_id) REFERENCES tags(id)
        )
        """)


async def save_comic_with_tags(db, date_str, relative_path, transcript, tags):
    existing_transcript = None
    async with db.execute(
        "SELECT transcript FROM comics WHERE date=?", (date_str,)
    ) as cur:
        row = await cur.fetchone()
        if row:
            existing_transcript = row[0]

    final_transcript = (
        transcript if (transcript and transcript.strip()) else existing_transcript
    )

    await db.execute(
        "INSERT OR REPLACE INTO comics (date, image_path, transcript, metadata_checked) VALUES (?, ?, ?, 1)",
        (date_str, str(relative_path), final_transcript),
    )

    if tags and len(tags) > 0:
        for tag in tags:
            await db.execute("INSERT OR IGNORE INTO tags (name) VALUES (?)", (tag,))
            async with db.execute("SELECT id FROM tags WHERE name=?", (tag,)) as cursor:
                row = await cursor.fetchone()
                tag_id = row[0]
            await db.execute(
                "INSERT OR IGNORE INTO comic_tags (comic_date, tag_id) VALUES (?, ?)",
                (date_str, tag_id),
            )
    await db.commit()


async def load_existing_dates(db, base_dir: Path):
    existing_dates = set()
    async with db.execute(
        "SELECT date, image_path, transcript, metadata_checked FROM comics"
    ) as cursor:
        async for row in cursor:
            date_str, image_path_db, transcript_db, metadata_checked = row
            has_image = False
            if image_path_db:
                try:
                    has_image = (base_dir / image_path_db).exists()
                except Exception:
                    has_image = False

            tag_count = 0
            async with db.execute(
                "SELECT COUNT(*) FROM comic_tags WHERE comic_date=?", (date_str,)
            ) as c:
                r = await c.fetchone()
                tag_count = r[0] if r else 0

            has_metadata = (
                (transcript_db and transcript_db.strip())
                or (tag_count > 0)
                or (metadata_checked and metadata_checked > 0)
            )
            if has_image and has_metadata:
                existing_dates.add(date_str)
            else:
                logger.info(
                    f"Incomplete data for {date_str}: image_exists={has_image}, has_metadata={bool(has_metadata)}; will reprocess"
                )
    return existing_dates

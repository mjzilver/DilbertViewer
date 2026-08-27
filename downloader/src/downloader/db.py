import logging
from pathlib import Path
import aiosqlite

logger = logging.getLogger(__name__)


async def create_tables(db: aiosqlite.Connection) -> None:
    await db.executescript("""
        CREATE TABLE IF NOT EXISTS comics (
            date TEXT PRIMARY KEY,
            image_path TEXT,
            transcript TEXT,
            metadata_checked INTEGER DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS tags (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE
        );

        CREATE TABLE IF NOT EXISTS comic_tags (
            comic_date TEXT,
            tag_id INTEGER,
            PRIMARY KEY (comic_date, tag_id),
            FOREIGN KEY (comic_date) REFERENCES comics(date),
            FOREIGN KEY (tag_id) REFERENCES tags(id)
        );
        """)


async def save_comic_with_tags(
    db: aiosqlite.Connection,
    date_str: str,
    relative_path: Path,
    transcript: str | None,
    tags: list[str],
) -> None:

    async with db.execute(
        "SELECT transcript FROM comics WHERE date=?",
        (date_str,),
    ) as cur:
        row = await cur.fetchone()

    existing_transcript = row[0] if row else None

    final_transcript = (
        transcript.strip() if transcript and transcript.strip() else existing_transcript
    )

    await db.execute(
        """
        INSERT OR REPLACE INTO comics
        (date, image_path, transcript, metadata_checked)
        VALUES (?, ?, ?, 1)
        """,
        (date_str, str(relative_path), final_transcript),
    )

    if tags:
        for tag in tags:
            await db.execute(
                "INSERT OR IGNORE INTO tags (name) VALUES (?)",
                (tag,),
            )

            async with db.execute(
                "SELECT id FROM tags WHERE name=?",
                (tag,),
            ) as cursor:
                row = await cursor.fetchone()
                tag_id = row[0]

            await db.execute(
                """
                INSERT OR IGNORE INTO comic_tags (comic_date, tag_id)
                VALUES (?, ?)
                """,
                (date_str, tag_id),
            )

    await db.commit()


async def load_existing_dates(db: aiosqlite.Connection, base_dir: Path):

    existing_dates = set()

    tag_counts = {}
    async with db.execute("""
        SELECT comic_date, COUNT(tag_id)
        FROM comic_tags
        GROUP BY comic_date
        """) as cursor:
        async for comic_date, count in cursor:
            tag_counts[comic_date] = count

    async with db.execute("""
        SELECT date, image_path, transcript, metadata_checked
        FROM comics
        """) as cursor:

        async for row in cursor:
            date_str, image_path_db, transcript_db, metadata_checked = row

            has_image = False
            if image_path_db:
                try:
                    has_image = (base_dir / image_path_db).exists()
                except Exception:
                    has_image = False

            tag_count = tag_counts.get(date_str, 0)

            has_metadata = (
                bool(transcript_db and transcript_db.strip())
                or tag_count > 0
                or bool(metadata_checked and metadata_checked > 0)
            )

            if has_image and has_metadata:
                existing_dates.add(date_str)
            else:
                logger.info(
                    f"Incomplete data for {date_str}: "
                    f"image_exists={has_image}, "
                    f"has_metadata={has_metadata}; reprocessing"
                )

    return existing_dates

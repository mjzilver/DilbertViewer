import datetime
import time
from dataclasses import dataclass, field


@dataclass
class ComicTask:
    date: datetime.date
    attempt: int = 0
    last_error: str | None = None
    need_image: bool = True
    need_metadata: bool = True


@dataclass
class WorkerProgress:
    status: str = "idle"
    current_date: str | None = None
    attempt: int = 0


@dataclass
class Progress:
    total: int = 0
    completed: int = 0
    failed: int = 0
    start_time: float = field(default_factory=time.monotonic)

    workers: dict[int, WorkerProgress] = field(default_factory=dict)

    @property
    def finished(self) -> int:
        return self.completed + self.failed

    @property
    def pending(self) -> int:
        return max(0, self.total - self.finished - self.active)

    @property
    def active(self) -> int:
        return sum(
            worker.status in ("active", "retrying") for worker in self.workers.values()
        )

    @property
    def retrying(self) -> int:
        return sum(worker.status == "retrying" for worker in self.workers.values())

    @property
    def percentage(self) -> float:
        if not self.total:
            return 100.0

        return self.finished / self.total * 100

    def worker(self, worker_id: int) -> WorkerProgress:
        return self.workers.setdefault(worker_id, WorkerProgress())

    def mark_active(self, worker_id: int, task: ComicTask) -> None:
        worker = self.worker(worker_id)
        worker.status = "active"
        worker.current_date = task.date.isoformat()
        worker.attempt = task.attempt

    def mark_retrying(self, worker_id: int, task: ComicTask) -> None:
        worker = self.worker(worker_id)
        worker.status = "retrying"
        worker.current_date = task.date.isoformat()
        worker.attempt = task.attempt

    def mark_idle(self, worker_id: int) -> None:
        worker = self.worker(worker_id)
        worker.status = "idle"
        worker.current_date = None
        worker.attempt = 0

    def mark_stopped(self, worker_id: int) -> None:
        worker = self.worker(worker_id)
        worker.status = "stopped"
        worker.current_date = None

    def mark_completed(self) -> None:
        self.completed += 1

    def mark_failed(self) -> None:
        self.failed += 1

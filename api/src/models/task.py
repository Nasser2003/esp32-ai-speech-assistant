from datetime import datetime
from databases.postgres_db import Base
from enum import Enum
from sqlalchemy import Enum as SQLEnum, String
from sqlalchemy.orm import Mapped, mapped_column

class TaskTypeEnum(str, Enum):
    ALARM = "ALARM"
    WAKE_UP_AI = "WAKE_UP_AI"
    CHANGE_VOLUME = "CHANGE_VOLUME"

class TaskStatusEnum(str, Enum):
    PENDING = "PENDING"
    IN_PROGRESS = "IN_PROGRESS"
    COMPLETED = "COMPLETED"
    CANCELED = "CANCELED"

class TaskType(Base):
    __tablename__ = "TASK_TYPES"
    id: Mapped[int] = mapped_column(primary_key=True, index=True)
    name: Mapped[str] = mapped_column(String, nullable=False)
    
class TaskStatus(Base):
    __tablename__ = "TASK_STATUSES"
    id: Mapped[int] = mapped_column(primary_key=True, index=True)
    name: Mapped[str] = mapped_column(String, nullable=False)

class Task(Base):
    __tablename__ = "TASKS"
    id: Mapped[int] = mapped_column(primary_key=True, index=True)
    device: Mapped[str] = mapped_column(String(31), nullable=False)
    run_at: Mapped[datetime] = mapped_column(nullable=False)
    type: Mapped[TaskTypeEnum] = mapped_column(SQLEnum(TaskTypeEnum), nullable=False)
    status: Mapped[TaskStatusEnum] = mapped_column(SQLEnum(TaskStatusEnum), nullable=False)
    argument: Mapped[str | None] = mapped_column(String, nullable=True)
    
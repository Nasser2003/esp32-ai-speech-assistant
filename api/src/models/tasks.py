from sqlalchemy import Boolean, Column, ForeignKey, Integer, String, DateTime
from databases.postgres_db import Base

class TaskType(Base):
    __tablename__ = "task_types"
    id = Column(Integer, primary_key=True, index=True)
    name = Column(String, nullable=False)
    
class TaskStatus(Base):
    __tablename__ = "task_statuses"
    id = Column(Integer, primary_key=True, index=True)
    name = Column(String, nullable=False)

class Task(Base):
    __tablename__ = "tasks"
    id = Column(Integer, primary_key=True, index=True)
    run_at = Column(DateTime(timezone=True), nullable=False)
    type = Column(Integer, ForeignKey("task_types.id"), nullable=False)
    status = Column(Integer, ForeignKey("task_statuses.id"), nullable=False)
    argument = Column(String, nullable=True)
    
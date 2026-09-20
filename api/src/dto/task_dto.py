from pydantic import BaseModel

from models.task import TaskStatusEnum

class TaskUpdate(BaseModel):
    task_status: TaskStatusEnum
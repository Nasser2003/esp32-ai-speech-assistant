from databases.postgres_db import PostgresDatabase
from models.task import Task, TaskStatusEnum, TaskTypeEnum
from datetime import datetime, timezone
from zoneinfo import ZoneInfo

class AiToolsManager:
    def __init__(self, db: PostgresDatabase):
        self.db = db
        self.tools = {
            "changeVolume": self.change_volume,
            "continueConversationLater": self.create_wakeup,
            "createAlarm": self.create_alarm,
        }
        self.esp32_args: dict = {}  # Placeholder for ESP32 info, can be set later if needed

    def set_esp32_info(self, esp32_info):
        self.esp32_args = esp32_info

    @staticmethod
    def _convert_to_server_time(time: str, user_timezone: str) -> datetime:
        """converts an user provided time in string format %Y-%m-%d %H:%M
        to a datetime object in UTC format."""
        user_time = datetime.strptime(time, "%Y-%m-%d %H:%M").replace(
            tzinfo=ZoneInfo(user_timezone)
        )
        return user_time.astimezone(timezone.utc)

    async def dispatch_tool(self, tool_name: str, *args, **kwargs):
        if tool_name not in self.tools:
            raise ValueError(f"Tool '{tool_name}' is not registered.")
        return await self.tools[tool_name](*args, **kwargs)

    async def change_volume(self, value: int):
        print(f"[TOOL] Changing volume to {value}")
        db = self.db.create_session()

        task = Task(
            device=self.esp32_args["mac"],
            run_at=datetime.now(timezone.utc),
            type=TaskTypeEnum.CHANGE_VOLUME,
            status=TaskStatusEnum.PENDING,
            argument=str(value),
        )
        db.add(task)
        db.commit()
        db.refresh(task)
        
        return {
            "success": True,
            "volume": value,
            "task_id": task.id,
        }

    async def create_wakeup(self, time: str, reason: str):
        print(
            f"[TOOL] Creating wake-up: "
            f"{time} - {reason}"
        )
        db = self.db.create_session()
        
        user_timezone = self.esp32_args.get("timezone", "Europe/Brussels")
        server_time = self._convert_to_server_time(time, user_timezone)

        task = Task(
            device=self.esp32_args["mac"],
            run_at=server_time.strftime("%Y-%m-%d %H:%M:%S"),
            type=TaskTypeEnum.WAKE_UP_AI,
            status=TaskStatusEnum.PENDING,
            argument="",
        )
        db.add(task)
        db.commit()
        db.refresh(task)

        return {
            "success": True,
            "time": time,
        }
        
    async def create_alarm(self, time: str):
        print(f"[TOOL] Creating alarm: "f"{time}")
        db = self.db.create_session()

        user_timezone = self.esp32_args.get("timezone", "Europe/Brussels")
        server_time = self._convert_to_server_time(time, user_timezone)

        task = Task(
            device=self.esp32_args["mac"],
            run_at=server_time.strftime("%Y-%m-%d %H:%M:%S"),
            type=TaskTypeEnum.ALARM,
            status=TaskStatusEnum.PENDING,
            argument="",
        )
        db.add(task)
        db.commit()
        db.refresh(task)

        return {
            "success": True,
            "time": user_time,
        }
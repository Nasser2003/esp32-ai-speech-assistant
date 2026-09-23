from datetime import datetime, timezone
from enum import Enum
from sqlalchemy import DateTime, Enum as SQLEnum, ForeignKey, Integer, String
from sqlalchemy.orm import Mapped, mapped_column, relationship

from databases.postgres_db import Base


class RoleEnum(str, Enum):
    USER = "user"
    SYSTEM = "system"
    ASSISTANT = "assistant"
    TOOL = "tool"


class ContextTypeEnum(str, Enum):
    SYSTEM = "system"
    SUMMARY = "summary"
    SUB_SYSTEM = "sub_system"


class SystemContext(Base):
    __tablename__ = "SYSTEMS"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True, index=True)
    type: Mapped[ContextTypeEnum] = mapped_column(
        SQLEnum(ContextTypeEnum, name="context_type", create_type=False, values_callable=lambda x: [e.value for e in x]),
        nullable=False,
    )
    content: Mapped[str] = mapped_column(String(5000), nullable=False)

    messages: Mapped[list["Message"]] = relationship(
        "Message", back_populates="system_context"
    )


class Message(Base):
    __tablename__ = "MESSAGES"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True, index=True)
    content: Mapped[str] = mapped_column(String(5000), nullable=False)
    role: Mapped[RoleEnum] = mapped_column(
        SQLEnum(RoleEnum, name="role", create_type=False, values_callable=lambda x: [e.value for e in x]),
        nullable=False,
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        default=lambda: datetime.now(timezone.utc),
        nullable=False,
    )
    system: Mapped[int] = mapped_column(
        Integer,
        ForeignKey("SYSTEMS.id", ondelete="NO ACTION", onupdate="NO ACTION"),
        nullable=False,
    )

    system_context: Mapped["SystemContext"] = relationship(
        "SystemContext", back_populates="messages"
    )

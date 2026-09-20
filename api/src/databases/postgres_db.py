from typing import Generator

from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker, declarative_base, Session

Base = declarative_base()

class PostgresDatabase:
    def __init__(self, host: str, port: int, user: str, password: str, database: str):
        db_url = f"postgresql://{user}:{password}@{host}:{port}/{database}"
        self.engine = create_engine(db_url)
        self.SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=self.engine)
        self.Base = Base

    def get_session(self):
        db = self.SessionLocal()
        try:
            yield db
        finally:
            db.close()
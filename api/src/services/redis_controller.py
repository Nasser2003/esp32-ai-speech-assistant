from os import name
from collections import Counter

import redis.asyncio as redis

class RedisController:
    def __init__(self, host='localhost', port=6379, db=0):
        self.redis_client = redis.Redis(host=host, port=port, db=db)

    async def setTTL(self, key, ttl):
        await self.redis_client.expire(key, ttl)

    async def r_push_expire(self, key, value, ttl):
        await self.redis_client.rpush(key, value)
        await self.redis_client.expire(key, ttl)
    
    async def blpop(self, key, timeout=0):
        return await self.redis_client.blpop(key, timeout)
    
    async def lpop(self, key):
        return await self.redis_client.lpop(key)

    async def get_majoritary(self, key):
        values = await self.redis_client.lrange(key, 0, -1)
        if not values:
            return None

        counter = Counter(values)
        return counter.most_common(1)[0][0].decode("utf-8")


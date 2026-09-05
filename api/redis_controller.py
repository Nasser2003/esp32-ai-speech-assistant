from os import name
from collections import Counter

import redis

class RedisController:
    def __init__(self, host='localhost', port=6379, db=0):
        self.redis_client = redis.Redis(host=host, port=port, db=db)

    def setTTL(self, key, ttl):
        self.redis_client.expire(key, ttl)

    def r_push_expire(self, key, value, ttl):
        self.redis_client.rpush(key, value)
        self.redis_client.expire(key, ttl)
    
    def blpop(self, key, timeout=0):
        return self.redis_client.blpop(key, timeout)
    
    def lpop(self, key):
        return self.redis_client.lpop(key)

    def get_majoritary(self, key):
        values = self.redis_client.lrange(key, 0, -1)
        if not values:
            return None

        counter = Counter(values)
        return counter.most_common(1)[0][0].decode("utf-8")


import redis

class RedisController:
    def __init__(self, host='localhost', port=6379, db=0):
        self.redis_client = redis.Redis(host=host, port=port, db=db)

    def setTTL(self, key, ttl):
        self.redis_client.expire(key, ttl)

    def rpush(self, key, value):
        self.redis_client.rpush(key, value)
    
    def blpop(self, key, timeout=0):
        return self.redis_client.blpop(key, timeout)
    
    def lpop(self, key):
        return self.redis_client.lpop(key)
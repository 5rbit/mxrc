import { RateLimiterMemory } from 'rate-limiter-flexible';
import type { FastifyRequest, FastifyReply } from 'fastify';
import type { RateLimiterResult } from '../types';

export function createRateLimiter(options: Partial<{
  points: number;
  duration: number;
  blockDuration: number;
}> = {}) {
  const defaults = {
    points: parseInt(process.env.RATE_LIMIT_POINTS || '10', 10),
    duration: parseInt(process.env.RATE_LIMIT_DURATION || '1', 10),
    blockDuration: 60,
  };
  return new RateLimiterMemory({ ...defaults, ...options });
}

export const apiRateLimiter = createRateLimiter({ points: 10, duration: 1, blockDuration: 60 });
export const healthCheckRateLimiter = createRateLimiter({ points: 100, duration: 1, blockDuration: 10 });
export const websocketRateLimiter = createRateLimiter({ points: 50, duration: 1, blockDuration: 30 });

export function rateLimiterMiddleware(
  limiter: RateLimiterMemory,
  keyGenerator: (request: FastifyRequest) => string = (request) => request.ip,
) {
  return async (request: FastifyRequest, reply: FastifyReply) => {
    const key = keyGenerator(request);
    try {
      await limiter.consume(key, 1);
    } catch (rateLimiterRes: any) {
      const retryAfter = Math.ceil(rateLimiterRes.msBeforeNext / 1000);
      return reply.code(429).header('Retry-After', retryAfter).send({
        error: 'RATE_LIMIT_EXCEEDED',
        message: 'Too many requests',
        retryAfter: rateLimiterRes.msBeforeNext,
      });
    }
  };
}

export const apiRateLimiterMiddleware = rateLimiterMiddleware(apiRateLimiter);
export const healthCheckRateLimiterMiddleware = rateLimiterMiddleware(healthCheckRateLimiter);

export async function checkWebSocketRateLimit(connectionId: string): Promise<RateLimiterResult> {
  try {
    await websocketRateLimiter.consume(connectionId, 1);
    return { allowed: true };
  } catch (rateLimiterRes: any) {
    return { allowed: false, retryAfter: rateLimiterRes.msBeforeNext };
  }
}

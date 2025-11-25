export function createCorsOptions(options: Record<string, any> = {}) {
  const defaults = {
    origin: process.env.NODE_ENV === 'production'
      ? process.env.CORS_ORIGIN?.split(',') || ['http://localhost:3000']
      : true,
    methods: ['GET', 'PUT', 'POST', 'DELETE', 'OPTIONS'],
    allowedHeaders: ['Content-Type', 'Authorization'],
    exposedHeaders: ['X-RateLimit-Limit', 'X-RateLimit-Remaining', 'Retry-After'],
    credentials: true,
    maxAge: 86400,
  };
  return { ...defaults, ...options };
}

export const getCorsOptions = () => createCorsOptions();

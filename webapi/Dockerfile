# Multi-stage build for MXRC WebAPI
# Optimized for production deployment with minimal image size

# Stage 1: Builder
FROM node:20-slim AS builder

WORKDIR /build

# Copy package files
COPY package*.json ./
COPY tsconfig.json ./

# Install dependencies (including dev dependencies for build)
RUN npm ci

# Copy source code
COPY src ./src

# Build TypeScript
RUN npm run build

# Prune dev dependencies
RUN npm prune --production

# Stage 2: Runtime
FROM node:20-slim

# Install dumb-init for proper signal handling
RUN apt-get update && apt-get install -y --no-install-recommends \
    dumb-init \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy built application and production dependencies
COPY --from=builder /build/dist ./dist
COPY --from=builder /build/node_modules ./node_modules
COPY --from=builder /build/package*.json ./

# Create non-root user
RUN useradd --system --no-create-home --shell /bin/false mxrc

# Change ownership
RUN chown -R mxrc:mxrc /app

# Switch to non-root user
USER mxrc

# Expose port (documentation only, actual port from ENV)
EXPOSE 8080

# Environment variables with defaults
ENV NODE_ENV=production \
    PORT=8080 \
    HOST=0.0.0.0 \
    LOG_LEVEL=info \
    IPC_SOCKET_PATH=/var/run/mxrc/ipc.sock \
    IPC_TIMEOUT_MS=5000 \
    SYSTEMD_NOTIFY=false

# Health check
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:${PORT}/api/health/live || exit 1

# Use dumb-init as PID 1 for proper signal handling
ENTRYPOINT ["dumb-init", "--"]

# Start application
CMD ["node", "dist/server.js"]

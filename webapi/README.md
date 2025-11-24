# MXRC Web API Server

Decoupled API server for monitoring and controlling MXRC Core Datastore via HTTP RESTful API and WebSocket.

## Features

- **HTTP RESTful API**: Read/write Datastore keys with JSON responses
- **WebSocket**: Real-time data updates and subscriptions
- **systemd Integration**: Service management with Watchdog support
- **Type Safety**: Zod schema validation based on IPC schema
- **Rate Limiting**: Token bucket algorithm for RT process protection
- **High Performance**: 1000+ req/s, < 128MB memory usage

## Prerequisites

- **OS**: Ubuntu 24.04 LTS (PREEMPT_RT)
- **Node.js**: 20 LTS or later
- **MXRC Core**: RT/Non-RT processes must be running
- **systemd**: Required for service management

## Quick Start

### 1. Install Dependencies

```bash
cd webapi
npm install
```

### 2. Configure Environment

```bash
cp .env.example .env
# Edit .env to match your system paths
```

### 3. Development

```bash
# With real MXRC Core
npm run dev

# With mock IPC server (no MXRC Core required)
npm run dev:mock
```

### 4. Verify Server

```bash
# Health check
curl http://localhost:3000/api/health

# Read Datastore key
curl http://localhost:3000/api/datastore/robot_position
```

## Testing

```bash
# All tests
npm test

# Unit tests only
npm run test:unit

# Integration tests
npm run test:integration

# Coverage report
npm run test:coverage
```

## Production Deployment

### 1. systemd Service

```bash
# Copy service file
sudo cp ../systemd/mxrc-webapi.service /etc/systemd/system/

# Enable and start
sudo systemctl enable mxrc-webapi
sudo systemctl start mxrc-webapi

# Check status
sudo systemctl status mxrc-webapi
```

### 2. View Logs

```bash
sudo journalctl -u mxrc-webapi -f
```

## API Documentation

For detailed API documentation, see:
- [OpenAPI Specification](../docs/specs/001-datastore-webui-api/contracts/openapi.yaml)
- [WebSocket Protocol](../docs/specs/001-datastore-webui-api/contracts/websocket.md)
- [Quickstart Guide](../docs/specs/001-datastore-webui-api/quickstart.md)

## Project Structure

```
webapi/
├── src/
│   ├── server.js           # Fastify entry point
│   ├── config/             # IPC schema loader
│   ├── ipc/                # Unix socket bridge
│   ├── routes/             # API endpoints
│   ├── middleware/         # Validation, rate limiting
│   └── utils/              # systemd notify, logging
├── tests/
│   ├── unit/               # Unit tests
│   ├── integration/        # Integration tests
│   └── mocks/              # Mock IPC server
└── package.json
```

## Architecture

- **IPC Mechanism**: Unix Domain Socket (not shared memory direct access)
- **HTTP Framework**: Fastify (45,140 req/s)
- **WebSocket Library**: ws (low overhead)
- **Type Validation**: Zod (TypeScript-first)
- **Rate Limiting**: rate-limiter-flexible (Token Bucket)

## Performance

- **HTTP**: 1000+ requests/second
- **WebSocket**: 100+ concurrent connections
- **Response Time**: < 1s for Datastore reads, < 100ms for health checks
- **Memory**: < 128MB
- **RT Impact**: < 1%

## License

ISC

## Support

For issues and questions, please refer to the project documentation in `docs/specs/001-datastore-webui-api/`.

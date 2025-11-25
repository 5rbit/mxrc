# WebSocket API Examples

## Connection

Connect to the WebSocket endpoint:

```javascript
const ws = new WebSocket('ws://localhost:3000/api/ws');

ws.on('open', () => {
  console.log('Connected to MXRC WebAPI');
});

ws.on('close', () => {
  console.log('Disconnected from MXRC WebAPI');
});

ws.on('error', (error) => {
  console.error('WebSocket error:', error);
});
```

## Subscribe to Keys

Subscribe to one or more datastore keys to receive real-time updates:

```javascript
// Subscribe to single key
ws.send(JSON.stringify({
  type: 'subscribe',
  key: 'system.status'
}));

// Subscribe to multiple keys
ws.send(JSON.stringify({
  type: 'subscribe',
  keys: ['system.status', 'device.temperature', 'motion.position']
}));
```

### Subscribe Response

```json
{
  "type": "subscribe",
  "data": [
    { "key": "system.status", "success": true },
    { "key": "device.temperature", "success": true },
    { "key": "motion.position", "success": true }
  ],
  "timestamp": "2025-01-24T10:30:00.000Z"
}
```

### Subscribe Error Response

```json
{
  "type": "subscribe",
  "data": [
    {
      "key": "invalid.key",
      "success": false,
      "error": "Key not found in schema"
    }
  ],
  "timestamp": "2025-01-24T10:30:00.000Z"
}
```

## Unsubscribe from Keys

Stop receiving updates for specific keys:

```javascript
// Unsubscribe from single key
ws.send(JSON.stringify({
  type: 'unsubscribe',
  key: 'system.status'
}));

// Unsubscribe from multiple keys
ws.send(JSON.stringify({
  type: 'unsubscribe',
  keys: ['system.status', 'device.temperature']
}));
```

### Unsubscribe Response

```json
{
  "type": "unsubscribe",
  "data": [
    { "key": "system.status", "success": true },
    { "key": "device.temperature", "success": true }
  ],
  "timestamp": "2025-01-24T10:30:00.000Z"
}
```

## Receive Notifications

When a subscribed key changes, you'll receive a notification:

```javascript
ws.on('message', (data) => {
  const message = JSON.parse(data.toString());

  if (message.type === 'notification') {
    console.log('Key changed:', message.key);
    console.log('New value:', message.data.value);
    console.log('Version:', message.data.version);
    console.log('Timestamp:', message.data.timestamp);
  }
});
```

### Notification Message Format

```json
{
  "type": "notification",
  "key": "device.temperature",
  "data": {
    "key": "device.temperature",
    "value": 25.5,
    "version": 42,
    "timestamp": "2025-01-24T10:30:00.000Z"
  },
  "timestamp": "2025-01-24T10:30:00.000Z"
}
```

## Error Handling

```javascript
ws.on('message', (data) => {
  const message = JSON.parse(data.toString());

  if (message.type === 'error') {
    console.error('Error:', message.error);
    console.error('Timestamp:', message.timestamp);
  }
});
```

### Error Message Format

```json
{
  "type": "error",
  "error": "No keys specified for subscription",
  "timestamp": "2025-01-24T10:30:00.000Z"
}
```

## Keepalive / Ping-Pong

The server automatically sends ping frames every 30 seconds to keep the connection alive. Most WebSocket clients handle pong responses automatically.

If you need to manually handle ping/pong:

```javascript
ws.on('ping', () => {
  ws.pong();
});
```

## Complete Example

```javascript
const WebSocket = require('ws');

const ws = new WebSocket('ws://localhost:3000/api/ws');

ws.on('open', () => {
  console.log('✓ Connected to MXRC WebAPI');

  // Subscribe to keys
  ws.send(JSON.stringify({
    type: 'subscribe',
    keys: ['system.status', 'device.temperature', 'motion.position']
  }));
});

ws.on('message', (data) => {
  const message = JSON.parse(data.toString());

  switch (message.type) {
    case 'subscribe':
      console.log('✓ Subscribed:', message.data);
      break;

    case 'notification':
      console.log(`📡 Update: ${message.key} = ${JSON.stringify(message.data.value)}`);
      break;

    case 'error':
      console.error('❌ Error:', message.error);
      break;

    default:
      console.log('Unknown message:', message);
  }
});

ws.on('close', () => {
  console.log('✗ Disconnected from MXRC WebAPI');
});

ws.on('error', (error) => {
  console.error('❌ WebSocket error:', error);
});

// Graceful shutdown
process.on('SIGINT', () => {
  console.log('\nShutting down...');

  // Unsubscribe before closing
  ws.send(JSON.stringify({
    type: 'unsubscribe',
    keys: ['system.status', 'device.temperature', 'motion.position']
  }));

  setTimeout(() => {
    ws.close();
    process.exit(0);
  }, 100);
});
```

## Browser Example

```html
<!DOCTYPE html>
<html>
<head>
  <title>MXRC WebAPI Monitor</title>
</head>
<body>
  <h1>MXRC Real-time Monitor</h1>
  <div id="status">Connecting...</div>
  <div id="data"></div>

  <script>
    const ws = new WebSocket('ws://localhost:3000/api/ws');
    const statusDiv = document.getElementById('status');
    const dataDiv = document.getElementById('data');

    ws.onopen = () => {
      statusDiv.textContent = 'Connected ✓';
      statusDiv.style.color = 'green';

      // Subscribe to keys
      ws.send(JSON.stringify({
        type: 'subscribe',
        keys: ['system.status', 'device.temperature']
      }));
    };

    ws.onmessage = (event) => {
      const message = JSON.parse(event.data);

      if (message.type === 'notification') {
        const div = document.createElement('div');
        div.textContent = `${message.key}: ${JSON.stringify(message.data.value)} (v${message.data.version})`;
        dataDiv.insertBefore(div, dataDiv.firstChild);
      }
    };

    ws.onclose = () => {
      statusDiv.textContent = 'Disconnected ✗';
      statusDiv.style.color = 'red';
    };

    ws.onerror = (error) => {
      statusDiv.textContent = 'Error ✗';
      statusDiv.style.color = 'red';
      console.error('WebSocket error:', error);
    };
  </script>
</body>
</html>
```

## Connection Statistics

Query WebSocket connection statistics:

```bash
curl http://localhost:3000/api/ws/stats
```

Response:

```json
{
  "totalConnections": 5,
  "totalSubscriptions": 12,
  "subscribedKeys": 8
}
```

## Message Types Summary

| Type | Direction | Description |
|------|-----------|-------------|
| `subscribe` | Client → Server | Subscribe to keys |
| `subscribe` | Server → Client | Subscription response |
| `unsubscribe` | Client → Server | Unsubscribe from keys |
| `unsubscribe` | Server → Client | Unsubscription response |
| `notification` | Server → Client | Data change notification |
| `error` | Server → Client | Error message |

## Best Practices

1. **Always handle errors**: Check for error messages and handle them appropriately
2. **Unsubscribe on disconnect**: Clean up subscriptions before closing the connection
3. **Reconnect logic**: Implement exponential backoff for reconnection attempts
4. **Message validation**: Always validate message format before processing
5. **Rate limiting**: Be mindful of subscription count and update frequency
6. **Connection monitoring**: Monitor ping/pong frames to detect connection issues

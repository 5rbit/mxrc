import * as notify from 'sd-notify';

interface SystemdNotifyOptions {
  enabled?: boolean;
  watchdogEnabled?: boolean;
}

export class SystemdNotify {
  private options: Required<SystemdNotifyOptions>;
  private watchdogInterval: NodeJS.Timeout | null = null;
  private healthCheckFn: (() => Promise<boolean>) | null = null;

  constructor(options: SystemdNotifyOptions = {}) {
    this.options = {
      enabled: process.env.SYSTEMD_NOTIFY === 'true',
      watchdogEnabled: process.env.WATCHDOG_ENABLED === 'true',
      ...options,
    };
  }

  ready(): void {
    if (this.options.enabled) {
      notify.ready();
    }
  }

  stopping(): void {
    if (this.options.enabled) {
      notify.stopping(0);
    }
  }

  status(message: string): void {
    // sd-notify doesn't have direct status method
    if (this.options.enabled) {
      // Just log for now
      console.log(`Status: ${message}`);
    }
  }

  watchdog(): void {
    if (this.options.enabled && this.options.watchdogEnabled) {
      notify.watchdog();
    }
  }

  startWatchdog(healthCheckFn?: () => Promise<boolean>): void {
    if (!this.options.enabled || !this.options.watchdogEnabled) {
      return;
    }

    const watchdogUsec = notify.watchdogInterval();
    if (watchdogUsec <= 0) return;

    this.healthCheckFn = healthCheckFn || null;
    const intervalMs = Math.floor(watchdogUsec / 2);

    this.watchdogInterval = setInterval(async () => {
      try {
        if (this.healthCheckFn) {
          const isHealthy = await this.healthCheckFn();
          if (!isHealthy) {
            this.stopWatchdog();
            return;
          }
        }
        notify.watchdog();
      } catch (error) {
        console.error('Health check error:', error);
        this.stopWatchdog();
      }
    }, intervalMs);
  }

  stopWatchdog(): void {
    if (this.watchdogInterval) {
      clearInterval(this.watchdogInterval);
      this.watchdogInterval = null;
    }
  }

  isEnabled(): boolean {
    return this.options.enabled;
  }

  isWatchdogEnabled(): boolean {
    return this.options.enabled && this.options.watchdogEnabled;
  }
}

export const systemdNotify = new SystemdNotify();

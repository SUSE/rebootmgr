# rebootmgr
RebootManager is a dbus service to execute a controlled reboot after updates in a defined maintenance window.

If you updated a system with e.g. transactional updates or a kernel update was applied, you can tell rebootmgrd with rebootmgrctl, that the machine should be reboot at the next possible time. This can either be immediately, during a defined maintenance window or, to avoid that a lot of machines boot at the same time, controlled with locks and etcd.

## Reboot Strategies

rebootmgr supports different strategies, when a reboot should be done:
* `instantly` - reboot immediately when the signal arrives.
* `maint-window` - reboot only during a specified maintenance window. If no window is specified, reboot immediately.
* `etcd-lock` - acquire a lock at etcd before reboot. If a maintenance window is specified, acquire the lock only during this window.
* `best-effort` - this is the default. If etcd is running, use `etcd-lock`. If no etcd is running, but a maintenance window is specified, use `maint-window`. If no maintenance window is specified, reboot immediately (`instantly`).
* `off` - rebootmgr continues to run, but ignores all signals to reboot. Setting the strategy to `off` does not clear the maintenance window. If rebootmgr is enabled again, it will continue to use the old specified maintenance window.

## Configuration example

File _/etc/rebootmgr.conf_
```
[rebootmgr]
window-start=3:30
window-duration=1h30m
strategy=best-effort
lock-group=default
```

## Checking if a reboot is requested

```bash
$ sudo rebootmgrctl status
Status: Reboot not requested
```

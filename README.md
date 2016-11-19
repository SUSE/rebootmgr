# rebootmgr
RebootManager is a dbus service to execute a controlled reboot after updates in a defined maintenance window.

If you updated a system with e.g. transactional updates or a kernel update was applied, you can tell rebootmgrd with rebootmgrctl, that the machine should be reboot at the next possible time. This can either be immeaditly, during a defined maintenance window or, to avoid that a lot of machines boot at the same time, controlled with locks and etcd.

## Reboot Strategies

rebootmgr supports different strategies, when a reboot should be done:
* `instantly` - reboot immeaditly when the signal arrives.
* `maint-window` - reboot only during a specified maintenance window. If no window is specified, reboot immeaditly.
* `etcd-lock` - acquire a lock at etcd before reboot. If a maintenance window is specified, acquire the lock only during this window.
* `best-efford` - this is the default. If etcd is running, use `etcd-lock`. If no etcd is running, but a maintenance window is specified, use `maint-window`. If no maintenance window is specified, reboot immeaditly (`instantly`).
* `off` - rebootmgr continues to run, but ignores all signals to reboot. Setting the strategy to `off` does not clear the maintenance window. If rebootmgr is eanbled again, it will continue to use the old specified maintenance window.


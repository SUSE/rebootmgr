# rebootmgr
RebootManager is a sd-varlink service to execute a controlled reboot after updates in a defined maintenance window.

If you updated a system with e.g. transactional updates or a kernel update was applied, you can tell rebootmgrd with rebootmgrctl, that the machine should be reboot at the next possible time. This can either be immediately or during a defined maintenance window. Beside a hard reboot this could also be a soft-reboot.

## Reboot Strategies

rebootmgr supports different strategies, when a reboot should be done:
* `instantly` - reboot immediately when the signal arrives.
* `maint-window` - reboot only during a specified maintenance window. If no window is specified, reboot immediately.
* `best-effort` - this is the default. If a maintenance window is specified, use `maint-window`. If no maintenance window is specified, reboot immediately (`instantly`).
* `off` - rebootmgr continues to run, but ignores all signals to reboot. Setting the strategy to `off` does not clear the maintenance window. If rebootmgr is enabled again, it will continue to use the old specified maintenance window.

## Configuration example

File _/etc/rebootmgr/rebootmgr.conf_
```
[rebootmgr]
window-start=3:30
window-duration=1h30m
strategy=best-effort
```

## Authorization

rebootmgrd authorizes every varlink method call via polkit. The caller's
root (UID 0) status always grants access immediately, without involving
polkit at all. For every other user, rebootmgrd asks polkit to check one
of the following actions:

| Action | Default | Description |
| --- | --- | --- |
| `org.openSUSE.rebootmgr.ping` | allow any | Check if rebootmgrd is alive |
| `org.openSUSE.rebootmgr.status` | allow any | Query the current reboot status |
| `org.openSUSE.rebootmgr.full-status` | allow any | Query the full status and configuration |
| `org.openSUSE.rebootmgr.metrics` | allow any | Read rebootmgrd metrics |
| `org.openSUSE.rebootmgr.reboot` | auth_admin_keep | Request a reboot or soft-reboot |
| `org.openSUSE.rebootmgr.cancel` | auth_admin_keep | Cancel a pending reboot |
| `org.openSUSE.rebootmgr.set-strategy` | auth_admin_keep | Change the configured reboot strategy |
| `org.openSUSE.rebootmgr.set-window` | auth_admin_keep | Change the configured maintenance window |
| `org.openSUSE.rebootmgr.quit` | auth_admin_keep | Stop the rebootmgrd daemon |
| `org.openSUSE.rebootmgr.set-log-level` | auth_admin_keep | Change the rebootmgrd log level |
| `org.openSUSE.rebootmgr.get-environment` | auth_admin_keep | Read the rebootmgrd process environment |

These defaults can be overridden locally with polkit rules (see
`polkit(8)`), e.g. to grant a specific user or group access without
requiring root or an admin password. If polkitd is not reachable, the
"allow any" actions above still succeed (same as before polkit support
was added), while the `auth_admin_keep` actions are denied for anyone but
root.

## Checking if a reboot is requested

```bash
$ sudo rebootmgrctl status
Status: Reboot not requested
```

## Metrics

rebootmgrd implements the `io.systemd.Metrics` varlink interface (as used by
`systemd-report`), exposing the following metric families:

| Name | Type | Description |
| --- | --- | --- |
| `org.openSUSE.rebootmgr.RebootStatus` | string | Whether a reboot or soft-reboot is requested, and if so, in which state it is |
| `org.openSUSE.rebootmgr.RebootMethod` | string | Which kind of reboot was requested (reboot or soft-reboot), if any |
| `org.openSUSE.rebootmgr.RebootScheduledTime` | gauge | Microseconds at which a pending reboot is scheduled for, if any |
| `org.openSUSE.rebootmgr.RebootStrategy` | string | Currently configured reboot strategy |
| `org.openSUSE.rebootmgr.RebootDisabled` | string | Whether reboots are temporarily disabled |
| `org.openSUSE.rebootmgr.MaintenanceWindowStart` | string | Start of the configured maintenance window as calendar specification |
| `org.openSUSE.rebootmgr.MaintenanceWindowDuration` | gauge | Duration of the configured maintenance window in seconds |
| `org.openSUSE.rebootmgr.Version` | string | Version of rebootmgr |

Metrics that depend on optional state (e.g. `RebootScheduledTime` or
`MaintenanceWindowStart`/`MaintenanceWindowDuration`) are omitted when that
state is not set.

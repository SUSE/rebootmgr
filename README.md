# rebootmgr
RebootManager is a dbus service to execute a controlled reboot after updates in a defined maintenance window.

If you updated a system with e.g. transactional updates or a kernel update was applied, you can tell rebootmgrd with rebootmgrctl, that the machine should be reboot at the next possible time. This can either be immeaditly, during a defined maintenance window or, to avoid that a lot of machines boot at the same time, controlled with locks and etcd.

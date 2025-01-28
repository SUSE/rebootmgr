//SPDX-License-Identifier: GPL-2.0-or-later

#include "varlink-org.openSUSE.rebootmgr.h"

/* XXX Implement SD_VARLINK_DEFINE_ENUM_VALUE ? */

static SD_VARLINK_DEFINE_METHOD(
		Reboot,
		SD_VARLINK_FIELD_COMMENT("Request a reboot"),
		SD_VARLINK_DEFINE_INPUT(Reboot, SD_VARLINK_INT,  0),
		SD_VARLINK_DEFINE_INPUT(Force, SD_VARLINK_BOOL, SD_VARLINK_NULLABLE),
		SD_VARLINK_DEFINE_OUTPUT(Method, SD_VARLINK_INT, 0),
		SD_VARLINK_DEFINE_OUTPUT(Scheduled, SD_VARLINK_STRING, 0));

static SD_VARLINK_DEFINE_METHOD(
		Cancel,
		SD_VARLINK_FIELD_COMMENT("Cancel a reboot"),
		SD_VARLINK_DEFINE_OUTPUT(Success, SD_VARLINK_BOOL, 0));

static SD_VARLINK_DEFINE_METHOD(
		SetStrategy,
		SD_VARLINK_FIELD_COMMENT("Set new strategy"),
		SD_VARLINK_DEFINE_INPUT(Strategy, SD_VARLINK_INT, 0),
		SD_VARLINK_DEFINE_OUTPUT(Success, SD_VARLINK_BOOL, 0));

static SD_VARLINK_DEFINE_METHOD(
		SetWindow,
		SD_VARLINK_FIELD_COMMENT("Set new maintenance window"),
		SD_VARLINK_DEFINE_INPUT(Start, SD_VARLINK_STRING, 0),
		SD_VARLINK_DEFINE_INPUT(Duration, SD_VARLINK_STRING, 0),
		SD_VARLINK_DEFINE_OUTPUT(Variable, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
		SD_VARLINK_DEFINE_OUTPUT(Success, SD_VARLINK_BOOL, 0));

static SD_VARLINK_DEFINE_METHOD(
		Status,
		SD_VARLINK_FIELD_COMMENT("If a reboot is requested and if yes, which kind of reboot"),
		SD_VARLINK_DEFINE_OUTPUT(RebootStatus, SD_VARLINK_INT, 0),
		SD_VARLINK_DEFINE_OUTPUT(RequestedMethod, SD_VARLINK_INT, SD_VARLINK_NULLABLE),
		SD_VARLINK_DEFINE_OUTPUT(RebootTime, SD_VARLINK_STRING, SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_METHOD(
		FullStatus,
		SD_VARLINK_FIELD_COMMENT("Provide full status of rebootmgr"),
		SD_VARLINK_DEFINE_OUTPUT(RebootStatus, SD_VARLINK_INT, 0),
		SD_VARLINK_DEFINE_OUTPUT(RebootStrategy, SD_VARLINK_INT, 0),
		SD_VARLINK_DEFINE_OUTPUT(RequestedMethod, SD_VARLINK_INT, SD_VARLINK_NULLABLE),
		SD_VARLINK_DEFINE_OUTPUT(RebootTime, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
		SD_VARLINK_DEFINE_OUTPUT(MaintenanceWindowStart, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
		SD_VARLINK_DEFINE_OUTPUT(MaintenanceWindowDuration, SD_VARLINK_INT, SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_METHOD(
		Quit,
		SD_VARLINK_FIELD_COMMENT("Stop the daemon"),
		SD_VARLINK_DEFINE_INPUT(ExitCode, SD_VARLINK_INT, SD_VARLINK_NULLABLE),
		SD_VARLINK_DEFINE_OUTPUT(Success, SD_VARLINK_BOOL, 0));

static SD_VARLINK_DEFINE_METHOD(
                Ping,
                SD_VARLINK_FIELD_COMMENT("Check if service is alive"),
                SD_VARLINK_DEFINE_OUTPUT(Alive, SD_VARLINK_BOOL, 0));

static SD_VARLINK_DEFINE_METHOD(
                SetLogLevel,
                SD_VARLINK_FIELD_COMMENT("Set debug and verbose mode, using BSD syslog log level integers."),
                SD_VARLINK_DEFINE_INPUT(Level, SD_VARLINK_INT, SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_METHOD(
                GetEnvironment,
                SD_VARLINK_FIELD_COMMENT("Returns the current environment block, i.e. the contents of environ[]."),
                SD_VARLINK_DEFINE_OUTPUT(Environment, SD_VARLINK_STRING, SD_VARLINK_NULLABLE|SD_VARLINK_ARRAY));

static SD_VARLINK_DEFINE_ERROR(InvalidParameter);
static SD_VARLINK_DEFINE_ERROR(AlreadyInProgress);
static SD_VARLINK_DEFINE_ERROR(NoRebootScheduled);
static SD_VARLINK_DEFINE_ERROR(ErrorWritingConfig);
static SD_VARLINK_DEFINE_ERROR(InternalError);

SD_VARLINK_DEFINE_INTERFACE(
                org_openSUSE_rebootmgr,
                "org.openSUSE.rebootmgr",
		SD_VARLINK_INTERFACE_COMMENT("Rebootmgr control APIs"),
		SD_VARLINK_SYMBOL_COMMENT("Request a reboot"),
                &vl_method_Reboot,
		SD_VARLINK_SYMBOL_COMMENT("Cancel a reboot"),
                &vl_method_Cancel,
		SD_VARLINK_SYMBOL_COMMENT("Set new strategy"),
                &vl_method_SetStrategy,
		SD_VARLINK_SYMBOL_COMMENT("Set new maintenance window"),
                &vl_method_SetWindow,
		SD_VARLINK_SYMBOL_COMMENT("Current status if a reboot got requested"),
                &vl_method_Status,
		SD_VARLINK_SYMBOL_COMMENT("Current status and configuration"),
                &vl_method_FullStatus,
		SD_VARLINK_SYMBOL_COMMENT("Stop the daemon"),
                &vl_method_Quit,
		&vl_method_Ping,
                SD_VARLINK_SYMBOL_COMMENT("Sets the maximum log level."),
                &vl_method_SetLogLevel,
                SD_VARLINK_SYMBOL_COMMENT("Get current environment block."),
                &vl_method_GetEnvironment,
		SD_VARLINK_SYMBOL_COMMENT("Invalid Parameter"),
                &vl_error_InvalidParameter,
		SD_VARLINK_SYMBOL_COMMENT("A reboot is already requested"),
                &vl_error_AlreadyInProgress,
                SD_VARLINK_SYMBOL_COMMENT("No Reboot was scheduled"),
                &vl_error_NoRebootScheduled,
		SD_VARLINK_SYMBOL_COMMENT("Writing new values in configuration file failed"),
		&vl_error_ErrorWritingConfig,
		SD_VARLINK_SYMBOL_COMMENT("Internal Error which should never happen"),
		&vl_error_InternalError);

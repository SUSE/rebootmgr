//SPDX-License-Identifier: GPL-2.0-or-later

/* Copyright (c) 2026 Thorsten Kukuk
   Author: Thorsten Kukuk <kukuk@suse.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, see <http://www.gnu.org/licenses/>. */

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-json.h>
#include <systemd/sd-varlink.h>

#include "basics.h"
#include "common.h"
#include "parse-duration.h"
#include "rebootmgrd-varlink-metrics.h"
#include "varlink-io.systemd.Metrics.h"

#define METRIC_ORG_OPENSUSE_REBOOTMGR_PREFIX "org.openSUSE.rebootmgr."

typedef enum MetricFamilyType {
  METRIC_FAMILY_TYPE_COUNTER,
  METRIC_FAMILY_TYPE_GAUGE,
  METRIC_FAMILY_TYPE_STRING,
  METRIC_FAMILY_TYPE_OBJECT,
} MetricFamilyType;

typedef struct MetricFamily MetricFamily;

typedef int (*metric_family_generate_func_t)(const MetricFamily *mf, sd_varlink *link, RM_CTX *ctx);

typedef struct MetricFamily {
  const char *name;
  const char *description;
  MetricFamilyType type;
  metric_family_generate_func_t generate;
} MetricFamily;

static const char *
metric_family_type_to_str(MetricFamilyType type)
{
  switch (type)
    {
    case METRIC_FAMILY_TYPE_COUNTER:
      return "counter";
    case METRIC_FAMILY_TYPE_GAUGE:
      return "gauge";
    case METRIC_FAMILY_TYPE_STRING:
      return "string";
    case METRIC_FAMILY_TYPE_OBJECT:
      return "object";
    default:
      return NULL;
    }
}

static int
metric_build_send(const MetricFamily *mf, sd_varlink *link, sd_json_variant *value)
{
  return sd_varlink_replybo(link,
			     SD_JSON_BUILD_PAIR_STRING("name", mf->name),
			     SD_JSON_BUILD_PAIR_VARIANT("value", value));
}

static int
metric_build_send_string(const MetricFamily *mf, sd_varlink *link, const char *value)
{
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;
  int r;

  r = sd_json_variant_new_string(&v, value);
  if (r < 0)
    return r;

  return metric_build_send(mf, link, v);
}

static int
metric_build_send_unsigned(const MetricFamily *mf, sd_varlink *link, uint64_t value)
{
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;
  int r;

  r = sd_json_variant_new_unsigned(&v, value);
  if (r < 0)
    return r;

  return metric_build_send(mf, link, v);
}

static int
reboot_status_generate(const MetricFamily *mf, sd_varlink *link, RM_CTX *ctx)
{
  const char *status;

  switch (ctx->reboot_status)
    {
    case RM_REBOOTSTATUS_NOT_REQUESTED:
      status = "not-requested";
      break;
    case RM_REBOOTSTATUS_REQUESTED:
      status = "requested";
      break;
    case RM_REBOOTSTATUS_WAITING_WINDOW:
      status = "waiting-window";
      break;
    default:
      status = "unknown";
      break;
    }

  return metric_build_send_string(mf, link, status);
}

static int
reboot_method_generate(const MetricFamily *mf, sd_varlink *link, RM_CTX *ctx)
{
  const char *str;

  if (ctx->reboot_method == RM_REBOOTMETHOD_UNKNOWN)
    return 0;

  if (rm_method_to_str(ctx->reboot_method, &str) < 0)
    return 0;

  return metric_build_send_string(mf, link, str);
}

static int
reboot_scheduled_time_generate(const MetricFamily *mf, sd_varlink *link, RM_CTX *ctx)
{
  if (ctx->reboot_status == RM_REBOOTSTATUS_NOT_REQUESTED || ctx->reboot_time == 0)
    return 0;

  return metric_build_send_unsigned(mf, link, ctx->reboot_time);
}

static int
reboot_requested_time_generate(const MetricFamily *mf, sd_varlink *link, RM_CTX *ctx)
{
  if (ctx->reboot_status == RM_REBOOTSTATUS_NOT_REQUESTED || ctx->reboot_time == 0 ||
      ctx->reboot_request_time == 0)
    return 0;

  return metric_build_send_unsigned(mf, link, ctx->reboot_request_time);
}

static int
reboot_strategy_generate(const MetricFamily *mf, sd_varlink *link, RM_CTX *ctx)
{
  const char *str;

  if (rm_strategy_to_str(ctx->reboot_strategy, &str) < 0)
    return 0;

  return metric_build_send_string(mf, link, str);
}

static int
reboot_disabled_generate(const MetricFamily *mf, sd_varlink *link, RM_CTX *ctx)
{
  return metric_build_send_string(mf, link, bool_to_str(ctx->temp_off));
}

static int
maint_window_start_generate(const MetricFamily *mf, sd_varlink *link, RM_CTX *ctx)
{
  _cleanup_(freep) char *str = NULL;

  if (ctx->maint_window_start == NULL)
    return 0;

  calendar_spec_to_string(ctx->maint_window_start, &str);
  if (str == NULL)
    return 0;

  return metric_build_send_string(mf, link, str);
}

static int
maint_window_duration_generate(const MetricFamily *mf, sd_varlink *link, RM_CTX *ctx)
{
  if (ctx->maint_window_duration == BAD_TIME)
    return 0;

  return metric_build_send_unsigned(mf, link, (uint64_t)ctx->maint_window_duration);
}

static int
rebootmgr_version_generate(const MetricFamily *mf, sd_varlink *link, RM_CTX _unused_(*ctx))
{
  return metric_build_send_string(mf, link, VERSION);
}

static const MetricFamily rebootmgr_metric_family_table[] = {
  {
    .name = METRIC_ORG_OPENSUSE_REBOOTMGR_PREFIX "RebootStatus",
    .description = "Whether a reboot or soft-reboot is requested, and if so, in which state it is",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = reboot_status_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_REBOOTMGR_PREFIX "RebootMethod",
    .description = "Which kind of reboot was requested (reboot or soft-reboot), if any",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = reboot_method_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_REBOOTMGR_PREFIX "RebootScheduledTime",
    .description = "Microseconds at which a pending reboot is scheduled for, if any",
    .type = METRIC_FAMILY_TYPE_GAUGE,
    .generate = reboot_scheduled_time_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_REBOOTMGR_PREFIX "RebootRequestTime",
    .description = "Microseconds at which a reboot was requested, if any",
    .type = METRIC_FAMILY_TYPE_GAUGE,
    .generate = reboot_requested_time_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_REBOOTMGR_PREFIX "RebootStrategy",
    .description = "Currently configured reboot strategy",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = reboot_strategy_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_REBOOTMGR_PREFIX "RebootDisabled",
    .description = "Whether reboots are temporarily disabled",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = reboot_disabled_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_REBOOTMGR_PREFIX "MaintenanceWindowStart",
    .description = "Start of the configured maintenance window as calendar specification",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = maint_window_start_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_REBOOTMGR_PREFIX "MaintenanceWindowDuration",
    .description = "Duration of the configured maintenance window in seconds",
    .type = METRIC_FAMILY_TYPE_GAUGE,
    .generate = maint_window_duration_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_REBOOTMGR_PREFIX "Version",
    .description = "Version of rebootmgr",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = rebootmgr_version_generate,
  },
  {}
};

int
vl_method_metrics_describe(sd_varlink *link, sd_json_variant *parameters,
                           sd_varlink_method_flags_t _unused_(flags), void _unused_(*userdata))
{
  int r;

  r = sd_varlink_dispatch(link, parameters, /* dispatch_table= */ NULL, /* userdata= */ NULL);
  if (r != 0)
    return r;

  r = sd_varlink_set_sentinel(link, "io.systemd.Metrics.NoSuchMetric");
  if (r < 0)
    return r;

  for (const MetricFamily *mf = rebootmgr_metric_family_table; mf->name; mf++)
    {
      _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;

      r = sd_json_buildo(&v,
			 SD_JSON_BUILD_PAIR_STRING("name", mf->name),
			 SD_JSON_BUILD_PAIR_STRING("description", mf->description),
			 SD_JSON_BUILD_PAIR_STRING("type", metric_family_type_to_str(mf->type)));
      if (r < 0)
	return r;

      r = sd_varlink_reply(link, v);
      if (r < 0)
	return r;
    }

  return 0;
}

int
vl_method_metrics_list(sd_varlink *link, sd_json_variant *parameters,
                       sd_varlink_method_flags_t _unused_(flags), void *userdata)
{
  RM_CTX *ctx = userdata;
  int r;

  r = sd_varlink_dispatch(link, parameters, /* dispatch_table= */ NULL, /* userdata= */ NULL);
  if (r != 0)
    return r;

  r = sd_varlink_set_sentinel(link, "io.systemd.Metrics.NoSuchMetric");
  if (r < 0)
    return r;

  for (const MetricFamily *mf = rebootmgr_metric_family_table; mf->name; mf++)
    {
      r = mf->generate(mf, link, ctx);
      if (r < 0)
	{
	  log_msg(LOG_DEBUG, "Failed to list metric family '%s': %s", mf->name, strerror(-r));
	  return r;
	}
    }

  return 0;
}

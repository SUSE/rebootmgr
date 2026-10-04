//SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern int vl_method_metrics_describe(sd_varlink *link,
	       			      sd_json_variant *parameters,
		                      sd_varlink_method_flags_t flags,
				      void *userdata);
extern int vl_method_metrics_list(sd_varlink *link,
		                  sd_json_variant *parameters,
                                  sd_varlink_method_flags_t flags,
				  void *userdata);

# SPDX-License-Identifier: GPL-2.0-only */
#
# Copyright (C) 2022-2025, Verdant Consultants, LLC.
#
#!/bin/bash

. $PITRAC_ROOT/ImageProcessing/RunScripts/runPiTracCommon.sh

#rm -f Logs/*.log

sudo -E nice -n -10 $PITRAC_ROOT/ImageProcessing/build/pitrac_lm  --system_mode camera2AutoCalibrate $PITRAC_COMMON_CMD_LINE_ARGS  --search_center_x 720 --search_center_y 500 --logging_level=trace --artifact_save_level=final_results_only

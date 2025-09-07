# SPDX-License-Identifier: GPL-2.0-only */
#
# Copyright (C) 2022-2025, Verdant Consultants, LLC.
#

#!/bin/bash

. $PITRAC_ROOT/ImageProcessing/RunScripts/runPiTracCommon.sh


#rm -f Logs/*.log

sudo -E nice -n -10 $PITRAC_ROOT/ImageProcessing/build/pitrac_lm  --run_single_pi --system_mode camera1_ball_location $PITRAC_COMMON_CMD_LINE_ARGS   --search_center_x 723 --search_center_y 544 --logging_level=info --artifact_save_level=all  --camera_gain 2.0

# SPDX-License-Identifier: GPL-2.0-only */
#
# Copyright (C) 2022-2025, Verdant Consultants, LLC.
#

. $PITRAC_ROOT/ImageProcessing/RunScripts/runPiTracCommon.sh

sudo -E nice -n -10 $PITRAC_ROOT/ImageProcessing/build/pitrac_lm --pulse_test  --run_single_pi  --system_mode camera1  --logging_level trace


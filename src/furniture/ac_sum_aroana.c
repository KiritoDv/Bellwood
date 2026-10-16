#include "assets/data/model/int_sum_aroana.h"
extern aFTR_rig_c aSumAroana_key_anime_data;

aFTR_rig_c aSumAroana_key_anime_data= {
    &cKF_bs_r_int_sum_aroana,
    &cKF_ba_r_int_sum_aroana,
    1.0f,
};

aFTR_PROFILE iam_sum_aroana = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&aSumAroana_key_anime_data,
	NULL,
	30.0f,
	0.01f,
	aFTR_SHAPE_TYPEA,
	mCoBG_FTR_TYPEA,
	0,
	2,
	0,
	aFTR_INTERACTION_FISH,
	NULL,
};

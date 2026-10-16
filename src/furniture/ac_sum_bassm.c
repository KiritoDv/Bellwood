#include "assets/data/model/int_sum_bassm.h"
extern aFTR_rig_c aSumBassm_key_anime_data;

aFTR_rig_c aSumBassm_key_anime_data= {
    &cKF_bs_r_int_sum_bassm,
    &cKF_ba_r_int_sum_bassm,
    1.0f,
};

aFTR_PROFILE iam_sum_bassm = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&aSumBassm_key_anime_data,
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

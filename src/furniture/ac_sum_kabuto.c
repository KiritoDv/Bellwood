#include "assets/data/model/int_sum_kabuto.h"
extern aFTR_rig_c aSumKabuto_key_anime_data;

aFTR_rig_c aSumKabuto_key_anime_data= {
    &cKF_bs_r_int_sum_kabuto,
    &cKF_ba_r_int_sum_kabuto,
    1.0f,
};

aFTR_PROFILE iam_sum_kabuto = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&aSumKabuto_key_anime_data,
	NULL,
	30.0f,
	0.01f,
	aFTR_SHAPE_TYPEA,
	mCoBG_FTR_TYPEA,
	0,
	2,
	0,
	aFTR_INTERACTION_INSECT,
	NULL,
};

#include "assets/data/model/int_sum_hirata.h"
extern aFTR_rig_c aSumHirata_key_anime_data;

aFTR_rig_c aSumHirata_key_anime_data= {
    &cKF_bs_r_int_sum_hirata,
    &cKF_ba_r_int_sum_hirata,
    1.0f,
};

aFTR_PROFILE iam_sum_hirata = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&aSumHirata_key_anime_data,
	NULL,
	30.0f,
	0.01f,
	aFTR_SHAPE_TYPEA,
	mCoBG_FTR_TYPEA,
	0,
	0,
	0,
	aFTR_INTERACTION_INSECT,
	NULL,
};

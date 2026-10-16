#include "libultra/gu.h"

void guMtxIdentF(float mf[4][4])
{
	int	i, j;

	for (i=0; i<4; i++)
	for (j=0; j<4; j++)
		if (i == j) mf[i][j] = 1.0;
		else mf[i][j] = 0.0;
}


void guMtxIdent(Mtx *m)
{
        float	mf[4][4];

	guMtxIdentF(mf);

	guMtxF2L(mf, m);
}


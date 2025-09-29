#pragma once

void matmul_ijk(float** A, float** B, float** C, size_t N)
{
	for (int i = 0; i < N; ++i)
	{
		for (int j = 0; j < N; ++j)
		{
			A[i][j] = 0.0f;
			for (int k = 0; k < N; ++k)
			{
				A[i][j] += B[i][k] * C[k][j];
			}
		}
	}
}

void matmul_kij(float** A, float** B, float** C, size_t N)
{
	for (int k = 0; k < N; ++k)
	{
		for (int i = 0; i < N; ++i)
		{
			for (int j = 0; j < N; ++j)
			{
				A[i][j] += B[i][k] * C[k][j];
			}
		}
	}
}

void matmul_stride_ijk(float** A, float** B, float** C, size_t N, size_t b)
{
	for (size_t i = 0; i < N; i += b)
	{
		for (size_t j = 0; j < N; j += b)
		{
			A[i][j] = 0.0f;
			for (size_t k = 0; k < N; k += b)
			{
				for (size_t ii = i; ii < i + b; ++ii)
				{
					for (size_t jj = j; jj < j + b; ++jj)
					{
						for (size_t kk = k; kk < k + b; ++kk)
						{
							A[ii][jj] += B[ii][kk] * C[kk][jj];
						}
					}
				}
			}
		}
	}
}

void matmul_stride_kij(float** A, float** B, float** C, size_t N, size_t b)
{
	for (size_t k = 0; k < N; k += b)
	{
		for (size_t i = 0; i < N; i += b)
		{
			for (size_t j = 0; j < N; j += b)
			{
				for (size_t ii = i; ii < i + b; ++ii)
				{
					for (size_t jj = j; jj < j + b; ++jj)
					{
						for (size_t kk = k; kk < k + b; ++kk)
						{
							A[ii][jj] += B[ii][kk] * C[kk][jj];
						}
					}
				}
			}
		}
	}
}
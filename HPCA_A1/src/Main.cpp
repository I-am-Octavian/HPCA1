#include <iostream>

#include "utils.h"
#include "matmul.h"

int main()
{
	constexpr int N = 4096;

	float** A;
	float** B;
	float** C;
	InitArray2D(A, N);
	InitArray2D(B, N);
	InitArray2D(C, N);

	RandomArrayGenerator2D(A, N);
	RandomArrayGenerator2D(B, N);

	LOOP2D(i, j, N)
		C[i][j] = 0.0f;

	float** A_new;
	float** B_new;
	float** C_new;
	InitArray2D(A_new, N);
	InitArray2D(B_new, N);
	InitArray2D(C_new, N);

	RandomArrayGenerator2D(A_new, N);
	RandomArrayGenerator2D(B_new, N);

	LOOP2D(i, j, N)
		C_new[i][j] = 0.0f;

	{
		Timer timer;
		matmul_ijk(A, B, C, N);
	}
	{
		Timer timer;
		matmul_stride_kij(A_new, B_new, C_new, N, 4);
	}

	std::cout << CheckEqual2D(C, C_new, N) << std::endl;

	delete[] A;
	delete[] B;
	delete[] C;

	delete[] A_new;
	delete[] B_new;
	delete[] C_new;

}
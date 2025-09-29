#pragma once
#include <random>
#include <chrono>
#include <iostream>

#define LOOP2D(i, j, N) for(size_t i = 0; i < N; ++i)\
for(size_t j = 0; j < N; ++j)

#ifdef __cplusplus
template<typename T>
void InitArray2D(T**& arr, size_t N)
{
	arr = new T * [N];
	for (size_t i = 0; i < N; ++i)
	{
		arr[i] = new T[N];
	}
}

template<typename T>
bool CheckEqual2D(T** A, T** B, size_t N)
{
	for (size_t i = 0; i < N; i++)
		for (size_t j = 0; j < N; j++)
			if (A[i][j] != B[i][j])
				return false;

	return true;
}


template<typename T>
void RandomArrayGenerator2D(T**& array, size_t N)
{
	constexpr size_t _MAX_ = 10000000;

	srand(time(NULL));

	for (size_t i = 0; i < N; ++i)
		for (size_t j = 0; j < N; ++j)
			array[i][j] = rand() % _MAX_;
}


#endif
unsigned int RandomArrayGenerator(int* array)
{
	constexpr size_t _MAX_ = 10000000;
	constexpr size_t _MAXNUM_ = 10000000;

	srand(time(NULL));
	// Number of array elements
	int NUM = 1000000;

#ifdef __cplusplus
	array = new int[NUM];
#else
	array = (int*)malloc(sizeof(int) * NUM);
#endif

	for (int j = 0; j < NUM; j++)
		array[j] = rand() % _MAX_;

	return NUM;
}

unsigned int RandomArrayGenerator(int array[], int NUM)
{
	constexpr size_t _MAX_ = 10000000;
	constexpr size_t _MAXNUM_ = 10000000;

	srand(time(NULL));
	// Number of array elements = NUM

	for (int j = 0; j < NUM; j++)
		array[j] = rand() % _MAX_;

	return NUM;
}

unsigned int RandomArrayGenerator(std::vector<int>& array, int NUM)
{
	constexpr size_t _MAX_ = 10000000;
	constexpr size_t _MAXNUM_ = 10000000;

	srand(time(NULL));
	// Number of array elements = NUM

	for (int j = 0; j < NUM; j++)
		array.push_back(rand() % _MAX_);

	return NUM;
}

class Timer
{
public:
	Timer()
	{
		m_start = std::chrono::high_resolution_clock::now();
	}
	~Timer()
	{
		m_end = std::chrono::high_resolution_clock::now();
		m_duration = m_end - m_start;

		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(m_duration).count(); // For milisec
		std::cout << "Execution Time : " << ms << "ms" << std::endl;
	}
private:
	std::chrono::high_resolution_clock::time_point m_start, m_end;
	std::chrono::duration<float> m_duration;
};


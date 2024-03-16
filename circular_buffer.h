#pragma once
#include "framework.h"
#define TEXT_SIZE 50
import std;

struct circular_buffer
{
	static constexpr int size = 10;
	std::wstring buffer[size];
	int time[size];
	int head{};
	int tail{};
	int element_count{};
	void Push(std::wstring c)
	{
		buffer[tail] = c;
		time[tail] = 0;
		tail = (tail + 1) % size;

		if (element_count != size)
			++element_count;
		else
			head = (head + 1) % size;
	}
	void Pop()
	{
		if (element_count > 0)
		{
			head = (head + 1) % size;
			--element_count;
		}
	}
	int GetFirstTime()
	{
		return time[head];
	}
	void IncreaseTime(int t)
	{
		for (int i = 0; i < size; i++)
		{
			time[i] += t;
		}
	}
};
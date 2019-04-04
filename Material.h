#pragma once
#include<string>
class Material
{
public:
	Material();
	~Material();

	std::wstring Name;
	int MatCBIndex = -1;
	int DiffuseSRVHeapIndex = -1;
	 
};


#pragma once

class FPakFileRef
{
public:
	FPakFileRef(const int32 InIndex) : Index(InIndex) {}

	int32 GetIndex() const { return Index; }

protected:
	int32 Index;
};
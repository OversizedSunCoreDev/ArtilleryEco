#pragma once
#include <optional>

template<typename DataOnlyTriviallyDestructibleValueType>
class FNoGuaranteeReadOnly
{
public:
	virtual ~FNoGuaranteeReadOnly() = default;
	virtual std::optional<DataOnlyTriviallyDestructibleValueType> peek(uint64_t input) = 0;
};


#include "genmc/CAT/CaatEvaluator.hpp"
#include "genmc/CAT/IncrementalEvaluator.hpp"
#include "genmc/CAT/LazyCycle.hpp"

#include <iostream>

int main()
{
	std::cout << "FixedPointStatistics=" << sizeof(cat::FixedPointStatistics) << '\n';
	std::cout << "CaatEvaluationResult=" << sizeof(cat::CaatEvaluationResult) << '\n';
	std::cout << "IncrementalStatistics=" << sizeof(cat::IncrementalStatistics) << '\n';
	std::cout << "IncrementalCaatEvaluator=" << sizeof(cat::IncrementalCaatEvaluator)
		  << '\n';
	std::cout << "LazyCycleStatistics=" << sizeof(cat::LazyCycleStatistics) << '\n';
#ifdef HAS_TOPOLOGICAL_CERTIFICATE
	std::cout << "LazyTopologicalOrder=" << sizeof(cat::LazyTopologicalOrder) << '\n';
	std::cout << "OptionalLazyTopologicalOrder="
		  << sizeof(std::optional<cat::LazyTopologicalOrder>) << '\n';
#endif
}

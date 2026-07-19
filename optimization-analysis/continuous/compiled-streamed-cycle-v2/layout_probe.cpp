#include "genmc/CAT/Analysis.hpp"
#include "genmc/CAT/CaatEvaluator.hpp"
#include "genmc/CAT/IncrementalEvaluator.hpp"
#include "genmc/CAT/LazyCycle.hpp"

#include <iostream>

int main()
{
	std::cout << "ModelAnalysis=" << sizeof(cat::ModelAnalysis) << '\n';
	std::cout << "FixedPointStatistics=" << sizeof(cat::FixedPointStatistics) << '\n';
	std::cout << "CaatEvaluationResult=" << sizeof(cat::CaatEvaluationResult) << '\n';
	std::cout << "IncrementalStatistics=" << sizeof(cat::IncrementalStatistics) << '\n';
	std::cout << "IncrementalCaatEvaluator=" << sizeof(cat::IncrementalCaatEvaluator)
		  << '\n';
	std::cout << "LazyCycleStatistics=" << sizeof(cat::LazyCycleStatistics) << '\n';
#ifdef HAS_STREAM_PLAN
	std::cout << "LazyStreamTerm=" << sizeof(cat::LazyStreamTerm) << '\n';
	std::cout << "LazyCyclePlan=" << sizeof(cat::LazyCyclePlan) << '\n';
	std::cout << "OptionalPredicateId="
		  << sizeof(std::optional<cat::PredicateId>) << '\n';
	std::cout << "OptionalLazyCyclePlan="
		  << sizeof(std::optional<cat::LazyCyclePlan>) << '\n';
#endif
}

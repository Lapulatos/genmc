/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 *
 * Apache License 2.0:
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * MIT License:
 *     https://opensource.org/licenses/MIT
 */

#include "genmc/Support/ThreadPool.hpp"
#include "genmc/Execution/DepExecutionGraph.hpp"
#ifdef BUILD_LLI
#include "Runtime/Interpreter.h"
#include "passes/LLIConfig.hpp"
#include "passes/LLVMModule.hpp"
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/Cloning.h>
#endif

ThreadPool::ThreadPool(const LLIConfig &lliConfig, const std::shared_ptr<const Config> &conf,
		       const std::unique_ptr<llvm::Module> &mod,
		       const std::unique_ptr<ModuleInfo> &MI, TFunT threadFun)
	: numWorkers_(lliConfig.threads), pinner_(numWorkers_), joiner_(workers_)
{

#ifndef BUILD_LLI
	UNREACHABLE(); /* unsupported */
#else
	/* Set global variables before spawning the threads */
	shouldHalt_.store(false);
	remainingTasks_.store(0);

	/* Have a non-empty queue before spawning workers */
	ExecutionGraph::Config dummyCfg{};
	dummyCfg.emitNALabels = conf->emitNALabels;
	auto execGraph = conf->isDepTrackingModel ? std::make_unique<DepExecutionGraph>(dummyCfg)
						  : std::make_unique<ExecutionGraph>(dummyCfg);
	auto exec = std::make_unique<GenMCDriver::Execution>(
		std::move(execGraph), std::move(WorkList()), std::move(ChoiceMap()),
		conf->scRvfExploration &&
				(conf->scRvfProgramSupported || conf->scRvfRegionalProgramEligible)
			? std::optional<genmc::rvf::Frame>(std::in_place)
			: std::nullopt);
	submit(std::move(exec));

	/* Spawn workers */
	for (auto i = 0U; i < numWorkers_; i++) {
		contexts_.push_back(std::make_unique<llvm::LLVMContext>());
		auto newmod = LLVMModule::cloneModule(mod, contexts_.back());
		auto newMI = MI->clone(*newmod);

		auto dw = GenMCDriver::create(conf, this);
		std::string buf;
		auto EE = llvm::Interpreter::create(std::move(newmod), std::move(newMI), &*dw,
						    &lliConfig, &buf);
		addWorker(i, std::move(dw), std::move(EE), threadFun);
	}
#endif
}

ThreadPool::~ThreadPool() { halt(); }

void ThreadPool::addWorker(unsigned int i, std::unique_ptr<GenMCDriver> driver,
			   std::unique_ptr<llvm::Interpreter> EE, TFunT threadFun)
{
#ifndef BUILD_LLI
	UNREACHABLE();
#else
	using ThreadT = std::packaged_task<VerificationResult(
		unsigned int, std::unique_ptr<GenMCDriver> driver,
		std::unique_ptr<llvm::Interpreter> EE, TFunT threadFun)>;

	ThreadT thread([this](unsigned int /*i*/, std::unique_ptr<GenMCDriver> driver,
			      std::unique_ptr<llvm::Interpreter> EE, TFunT threadFun) {
		std::optional<VerificationResult> aggregate;
		while (true) {
			auto taskUP = popTask();

			/* If the state is empty, nothing left to do */
			if (!taskUP)
				break;

			const auto queuedToken = taskUP->regionToken;
			auto completionToken = queuedToken;
			VerificationResult taskResult;
			if (shouldExecuteRegionTask(queuedToken)) {
				/* Prepare the driver and start the exploration */
				driver->initFromState(std::move(taskUP));
				threadFun(&*driver, &*EE);
				/* A tokenless task may open a region at its first quotient merge. It
				 * then becomes the transaction's first running descendant. */
				completionToken = driver->getCurrentRegionToken();
				taskResult = driver->takeTaskResult();
			}
			if (auto publish = completeTask(completionToken, std::move(taskResult))) {
				if (aggregate)
					*aggregate += std::move(*publish);
				else
					aggregate.emplace(std::move(*publish));
				driver->seedTaskWarnings(aggregate->warnings);
			}
			if (shouldHalt() || getRemainingTasks() == 0)
				break;
		}
		return aggregate ? std::move(*aggregate) : VerificationResult{};
	});

	results_.push_back(std::move(thread.get_future()));

	workers_.emplace_back(std::move(thread), i, std::move(driver), std::move(EE),
			      std::move(threadFun));
	pinner_.pin(workers_.back(), i);
#endif
}

#ifdef BUILD_LLI
void ThreadPool::submit(ThreadPool::TaskT t)
{
	std::lock_guard<std::mutex> lock(stateMtx_);
	if (t->regionToken) {
		auto region = regions_.find(t->regionToken->id);
		if (region == regions_.end() ||
		    !region->second->transaction.tryAddDescendant(*t->regionToken))
			return;
	}
	incRemainingTasks();
	queue_.push(std::move(t));
	stateCV_.notify_one();
}
#endif

auto ThreadPool::beginRegion(TaskT nativeEntry, VerificationResult durablePrefix)
	-> genmc::rvf::RegionToken
{
	std::lock_guard<std::mutex> lock(stateMtx_);
	const genmc::rvf::RegionToken token{nextRegionId_++, 1};
	auto [region, inserted] = regions_.emplace(
		token.id, std::make_unique<RegionRecord>(token, std::move(nativeEntry),
							std::move(durablePrefix)));
	VERIFY(inserted, "regional transaction id collision");
	VERIFY(region->second->transaction.tryAddDescendant(token),
	       "failed to claim region-opening task");
	return token;
}

auto ThreadPool::requestRegionRevocation(genmc::rvf::RegionToken token, std::string reason) -> bool
{
	std::lock_guard<std::mutex> lock(stateMtx_);
	auto region = regions_.find(token.id);
	if (region == regions_.end())
		return false;
	auto transition =
		region->second->transaction.requestRevocation(token, std::move(reason));
	if (!transition.accepted)
		return false;
	/* The region-opening task owns one outstanding descendant until completeTask().
	 * A revocation request therefore cannot be the transition that schedules replay:
	 * completeTask() must first publish the durable pre-region prefix. */
	VERIFY(!transition.replayNative,
	       "regional revocation bypassed durable-prefix publication");
	if (transition.replayNative) {
		incRemainingTasks();
		queue_.push(std::move(region->second->nativeEntry));
		regions_.erase(region);
		stateCV_.notify_one();
	}
	return true;
}

auto ThreadPool::shouldExecuteRegionTask(
	const std::optional<genmc::rvf::RegionToken> &token) -> bool
{
	if (!token)
		return true;
	std::lock_guard<std::mutex> lock(stateMtx_);
	auto region = regions_.find(token->id);
	return region != regions_.end() &&
	       region->second->transaction.isActive(*token);
}

auto ThreadPool::completeTask(const std::optional<genmc::rvf::RegionToken> &token,
			      VerificationResult result) -> std::optional<VerificationResult>
{
	std::optional<VerificationResult> publish;
	std::lock_guard<std::mutex> lock(stateMtx_);
	if (!token) {
		publish.emplace(std::move(result));
	} else if (auto region = regions_.find(token->id); region != regions_.end()) {
		auto &record = *region->second;
		if (record.transaction.isActive(*token)) {
			if (record.speculativeResult)
				*record.speculativeResult += std::move(result);
			else
				record.speculativeResult.emplace(std::move(result));
		}
		auto transition = record.transaction.retireDescendant(*token);
		if (transition.publish) {
			publish.emplace(std::move(record.durablePrefixResult));
			if (record.speculativeResult)
				*publish += std::move(*record.speculativeResult);
			regions_.erase(region);
		} else if (transition.replayNative) {
			publish.emplace(std::move(record.durablePrefixResult));
			incRemainingTasks();
			queue_.push(std::move(record.nativeEntry));
			regions_.erase(region);
			stateCV_.notify_one();
		}
	}
	if (decRemainingTasks() == 0)
		stateCV_.notify_all();
	return publish;
}

auto ThreadPool::tryPopPoolQueue() -> ThreadPool::TaskT { return queue_.tryPop(); }

auto ThreadPool::tryStealOtherQueue() -> ThreadPool::TaskT
{
	/* TODO: Implement work-stealing */
	return nullptr;
}

auto ThreadPool::popTask() -> ThreadPool::TaskT
{
	while (true) {
		if (shouldHalt())
			return nullptr;
		if (auto t = tryPopPoolQueue()) {
			if (shouldHalt())
				return nullptr;
			return t;
		}
		if (auto t = tryStealOtherQueue()) {
			if (shouldHalt())
				return nullptr;
			return t;
		}

		std::unique_lock<std::mutex> lock(stateMtx_);
		if (shouldHalt() || getRemainingTasks() == 0)
			return nullptr;
		stateCV_.wait(lock);
	}
	return nullptr;
}

auto ThreadPool::waitForTasks() -> std::vector<std::future<VerificationResult>>
{
	while (!shouldHalt() && getRemainingTasks() > 0)
		std::this_thread::yield();

	return std::move(results_);
}

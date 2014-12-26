#include <cstdlib>
#include <iostream>

#include "lpu_management.h"
#include "structure.h"

/********************************************  LPU Counter  ***********************************************/

LpuCounter::LpuCounter(int lpsDimensions) {
	
	this->lpsDimensions = lpsDimensions;
	
	lpuCounts = new int[lpsDimensions];
	lpusUnderDimensions = new int[lpsDimensions];
	currentLpuId = new int[lpsDimensions];
	currentLinearLpuId = INVALID_ID;

	currentRange = new LpuIdRange;
	currentRange->startId = INVALID_ID;
	currentRange->endId = INVALID_ID;
}

void LpuCounter::setLpuCounts(int lpuCounts[]) {
	for (int i = 0; i < lpsDimensions; i++) {
		this->lpuCounts[i] = lpuCounts[i];
	}
	int lpusUnderDim = 1;
	for (int j = lpsDimensions - 1; j >= 0; j--) {
		lpusUnderDim *= lpuCounts[j];
		lpusUnderDimensions[j] = lpusUnderDim;
	} 
}

void LpuCounter::setCurrentRange(PPU_Ids ppuIds) {
	
	int totalLpus = 1;
	for (int i = 0; i < lpsDimensions; i++) {
		totalLpus *= lpuCounts[i];
	}

	int ppuCount = ppuIds.ppuCount;
	int lpusPerPpu = totalLpus / ppuCount;
	int extraLpus = totalLpus % ppuCount;
	int groupId = ppuIds.groupId;
	
	currentRange->startId = lpusPerPpu * groupId;
	currentRange->endId = currentRange->startId + lpusPerPpu - 1;
	if (groupId == ppuCount - 1) {
		currentRange->endId = currentRange->endId + extraLpus; 
	}
}

int *LpuCounter::setCurrentCompositeLpuId(int linearId) {
	int remaining = linearId;
	for (int i = 0; i < lpsDimensions - 1; i++) {
		currentLpuId[i] = remaining / lpusUnderDimensions[i];
		remaining = remaining % lpusUnderDimensions[i];
	}
	currentLpuId[lpsDimensions - 1] = remaining;
	currentLinearLpuId = linearId;
	return currentLpuId;
}

int LpuCounter::getNextLpuId(int previousLpuId) {
	if (previousLpuId == INVALID_ID) {
		return currentRange->startId;
	} else {
		int candidateNext = previousLpuId + 1;
		if (candidateNext > currentRange->endId) return INVALID_ID;
		else return candidateNext;
	}
}

void LpuCounter::resetCounter() {
	currentRange->startId = INVALID_ID;
	currentRange->endId = INVALID_ID;
	currentLinearLpuId = INVALID_ID;
}

/*********************************************  LPS State  ************************************************/

LpsState::LpsState(int lpsDimensions) {
	counter = new LpuCounter(lpsDimensions);
	checkpointed = false;
	currentLpu = NULL;
}

/*******************************************  Thread State  ***********************************************/

ThreadState::ThreadState(int lpsCount, int *lpsDimensions, int *partitionArgs, ThreadIds *threadIds) {
	this->lpsCount = lpsCount;
	lpsStates = new LpsState*[lpsCount];
	for (int i = 0; i < lpsCount; i++) {
		lpsStates[i] = new LpsState(lpsDimensions[i]);
	}
	lpsParentIndexMap = NULL;
	this->partitionArgs = partitionArgs;
	this->threadIds = threadIds;
}

LPU *ThreadState::getNextLpu(int lpsId, int containerLpsId, int currentLpuId) {
	
	// this is not the first call to get next LPU when current Id is valid
	if (currentLpuId != INVALID_ID) {
		LpsState *state = lpsStates[lpsId];
		LpuCounter *counter = state->getCounter();
		int nextLpuId = counter->getNextLpuId(currentLpuId);
		// if next LPU is valid then just compute it, update LPS state and return the LPU to the caller
		if (nextLpuId != INVALID_ID) {
			int *compositeId = counter->setCurrentCompositeLpuId(nextLpuId);
			int *lpuCounts = counter->getLpuCounts();
			LPU *lpu = computeNextLpu(lpsId, lpuCounts, compositeId);
			state->setCurrentLpu(lpu);
			return lpu;
		// otherwise there is a possible need for recursively going up in the LPS hierarchy and 
		// recompute some parent LPUs before we can decide if anymore LPUs are there in the current LPS
		// to execute	
		} else {
			// reset state regardless of subsequent changes			
			counter->resetCounter();
			state->setCurrentLpu(NULL);
			
			// if parent is checkpointed then there is no further scope for recursion and we should
			// declare that no new LPUs to be executed
			int parentLpsId = lpsParentIndexMap[lpsId];
			LpsState *parentState = lpsStates[parentLpsId];
			if (parentState->isCheckpointed()) {
				return NULL;
			// otherwise check if parent LPS'es LPU can be updated
			} else {			
				LpuCounter *parentCounter = parentState->getCounter();
				int parentLpuId = parentCounter->getCurrentLpuId();

				// recursively call the same routine in the parent LPS to update the parent LPU
				// if possible
				LPU *parentLpu = getNextLpu(parentLpsId, containerLpsId, parentLpuId);
				
				// If parent LPU is NULL then it means all parent LPUs have been executed too. So
				// there is nothing more to do in current LPS either 
				if (parentLpu == NULL) return NULL;
				
				// Otherwise, counters for current LPS should be reset, the range of LPUs that 
				// current thread needs to execute should also be renewed
				int *newLpuCounts = computeLpuCounts(lpsId);
				counter->setLpuCounts(newLpuCounts);
				counter->setCurrentRange(threadIds->ppuIds[lpsId]);

				// finally, compute next LPU to execute, save state, and return the LPU
				nextLpuId = counter->getNextLpuId(INVALID_ID);
				int *compositeId = counter->setCurrentCompositeLpuId(nextLpuId);
				int *lpuCounts = counter->getLpuCounts();
				LPU *lpu = computeNextLpu(lpsId, lpuCounts, compositeId);
				state->setCurrentLpu(lpu);
				return lpu;	
			}
		}
	}

	// this is the first call to the routine if current ID is invalid  
	LpsState *containerState = lpsStates[containerLpsId];
	// checkpoint the container state to limit the span of recursive get-Next-LPU call below that LPS
	containerState->checkpointState();
	
	// check if parent LPS is the same as the container LPS	
	int parentLpsId = lpsParentIndexMap[lpsId];
	LpsState *parentState = lpsStates[parentLpsId];
	// if they are not the same then do a recursive get-Next_LPU call on the parent to initiate parent's counter
	if (containerLpsId != parentLpsId && parentLpsId != INVALID_ID) {
		getNextLpu(parentLpsId, containerLpsId, INVALID_ID);
	}

	// initiate LPU counter and range variables
	LpsState *state = lpsStates[lpsId];
	LpuCounter *counter = state->getCounter();
	int *newLpuCounts = computeLpuCounts(lpsId);
	counter->setLpuCounts(newLpuCounts);
	counter->setCurrentRange(threadIds->ppuIds[lpsId]);
				
	// finally, compute next LPU to execute, save state, and return the LPU
	int nextLpuId = counter->getNextLpuId(INVALID_ID);
	int *compositeId = counter->setCurrentCompositeLpuId(nextLpuId);
	int *lpuCounts = counter->getLpuCounts();
	LPU *lpu = computeNextLpu(lpsId, lpuCounts, compositeId);
	state->setCurrentLpu(lpu);
	return lpu;	
}

void ThreadState::removeCheckpoint(int lpsId) {
	LpsState *state = lpsStates[lpsId];
	state->removeCheckpoint();
}

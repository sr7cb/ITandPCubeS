#include <iostream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include "list.h"
#include "../structure.h"
#include "interval.h"
#include "partition.h"

using namespace std;

//------------------------------------------------ Partition Instruction -------------------------------------------------

PartitionInstr::PartitionInstr(const char *n, Dimension pd, int id, int count, bool r) {
	name = n;
	parentDim = pd;
	partId = id;
	partsCount = count;
	reorderIndex = r;
	prevInstr = NULL;
}

List<IntervalSeq*> *PartitionInstr::getTrueIntervalDesc() {
	List<IntervalSeq*> *intervalList = new List<IntervalSeq*>;
	getIntervalDesc(intervalList);
	return intervalList;
}

void PartitionInstr::drawTrueIntervalDesc(Dimension dimension, int labelGap) {
	DrawingLine *drawingLine = new DrawingLine(dimension, labelGap);
	List<IntervalSeq*> *intervals = getTrueIntervalDesc();
	for (int i = 0; i < intervals->NumElements(); i++) {
		intervals->Nth(i)->draw(drawingLine);
	}
	drawingLine->draw();
	while (intervals->NumElements() > 0) {
		IntervalSeq *interval = intervals->Nth(0);
		intervals->RemoveAt(0);
		delete interval;
	}
	delete intervals;
	delete drawingLine;
}

void PartitionInstr::getIntervalDesc(List<IntervalSeq*> *descInConstruct) {
	if (descInConstruct->NumElements() == 0) {
		List<IntervalSeq*> *intervalDesc = getIntervalDesc();
		descInConstruct->AppendAll(intervalDesc);
		while (intervalDesc->NumElements() > 0) intervalDesc->RemoveAt(0);
		delete intervalDesc;
	}
	if (prevInstr != NULL) {
		prevInstr->getIntervalDesc(descInConstruct);
	}
}

void PartitionInstr::drawIntervals() {
	if (prevInstr != NULL) {
		prevInstr->drawIntervals();
	}
	cout << "\n" << name << "\n";
	DrawingLine *drawingLine = new DrawingLine(parentDim, 10);
	List<IntervalSeq*> *intervalList = getIntervalDesc();
	for (int i = 0; i < intervalList->NumElements(); i++) {
		intervalList->Nth(i)->draw(drawingLine);
	}
	drawingLine->draw();
}

//------------------------------------------------------ Block Size ------------------------------------------------------

BlockSizeInstr::BlockSizeInstr(Dimension pd, int id, int size)
: PartitionInstr("Block-Size", pd, id, 0, false) {
	int dimLength = pd.length;
	this->partsCount = (dimLength + size - 1) / size;
	this->size = size;
}

Dimension BlockSizeInstr::getDimension() {
	int begin = parentDim.range.min + partId * size;
	int remaining = parentDim.range.max - begin + 1;
	int intervalLength = (remaining >= size) ? size : remaining;
	Dimension partDimension;
	partDimension.range.min = begin;
	partDimension.range.max = begin + intervalLength - 1;
	partDimension.length = intervalLength;
	return partDimension;
}

List<IntervalSeq*> *BlockSizeInstr::getIntervalDesc() {
	List<IntervalSeq*> *list = new List<IntervalSeq*>;
	Dimension partDim = getDimension();
	int begin = partDim.range.min;
	int length = partDim.length;
	int period = length;
	int count = 1;
	IntervalSeq *interval = new IntervalSeq(begin, length, period, count);
	list->Append(interval);
	return list;
}

int BlockSizeInstr::calculatePartsCount(Dimension dimension, bool updateProperties) {
	int count = (dimension.length + size - 1) / size;
	if (updateProperties) {
		this->parentDim = dimension;
		this->partsCount = count;
	}
	return count;
}

List<IntervalSeq*> *BlockSizeInstr::getIntervalDescForRange(Range idRange) {
	List<IntervalSeq*> *list = new List<IntervalSeq*>;
	int partId = this->partId;
	this->partId = idRange.min;
	Dimension startDim = getDimension();
	this->partId = idRange.max;
	Dimension endDim = getDimension();
	Dimension rangeDim = Dimension();
	rangeDim.range.min = startDim.range.min;
	rangeDim.range.max = endDim.range.max;
	rangeDim.setLength();
	IntervalSeq *interval = new IntervalSeq(rangeDim.range.min, rangeDim.length, rangeDim.length, 1);
	list->Append(interval);
	this->partId = partId;
	return list;
}

void BlockSizeInstr::getIntervalDescForRangeHierarchy(List<Range> *rangeList, List<IntervalSeq*> *descInConstruct) {
	Range idRange = rangeList->Nth(rangeList->NumElements() - 1);
	if (descInConstruct->NumElements() == 0) {
		List<IntervalSeq*> *myIntervalList = getIntervalDescForRange(idRange);
		descInConstruct->AppendAll(myIntervalList);
		delete myIntervalList;
	} else {
		List<IntervalSeq*> *updatedList = new List<IntervalSeq*>;
		int iterationCount = idRange.max - idRange.min + 1;
		int partId = this->partId;
		this->partId = idRange.min;
		int period = getDimension().length;
		for (int i = 0; i < descInConstruct->NumElements(); i++) {
			IntervalSeq *subInterval = descInConstruct->Nth(i);
			if (subInterval->count == 1) {
				subInterval->count = iterationCount;
				subInterval->period = period;
				updatedList->Append(subInterval);
			} else {
				for (int j = 0; j < subInterval->count; j++) {
					int newBegin = subInterval->begin + subInterval->period * j;
					IntervalSeq *newInterval = new IntervalSeq(newBegin, subInterval->length, period, iterationCount);
					updatedList->Append(newInterval);
				}
			}
		}
		this->partId = partId;
		descInConstruct->clear();
		descInConstruct->AppendAll(updatedList);
		delete updatedList;
	}
	rangeList->RemoveAt(rangeList->NumElements() - 1);
	if (prevInstr != NULL) prevInstr->getIntervalDescForRangeHierarchy(rangeList, descInConstruct);
}

//----------------------------------------------------- Block Count ------------------------------------------------------

BlockCountInstr::BlockCountInstr(Dimension pd, int id, int count)
: PartitionInstr("Block-Count", pd, id, 0, false) {
	int length = pd.length;
	this->partsCount = max(1, min(count, length));
	this->count = count;
}

Dimension BlockCountInstr::getDimension() {
	int size = parentDim.length / count;
	int begin = partId * size;
	int length = (partId < count - 1) ? size : parentDim.range.max - begin + 1;
	Dimension partDimension;
	partDimension.range.min = begin;
	partDimension.range.max = begin + length - 1;
	partDimension.length = length;
	return partDimension;
}

List<IntervalSeq*> *BlockCountInstr::getIntervalDesc() {
	List<IntervalSeq*> *list = new List<IntervalSeq*>;
	Dimension partDim = getDimension();
	int begin = partDim.range.min;
	int length = partDim.length;
	int period = length;
	int count = 1;
	IntervalSeq *interval = new IntervalSeq(begin, length, period, count);
	list->Append(interval);
	return list;
}

int BlockCountInstr::calculatePartsCount(Dimension dimension, bool updateProperties) {
	int count = max(1, min(count, dimension.length));
	if (updateProperties) {
		this->parentDim = dimension;
		this->partsCount = count;
	}
	return count;
}

List<IntervalSeq*> *BlockCountInstr::getIntervalDescForRange(Range idRange) {
	List<IntervalSeq*> *list = new List<IntervalSeq*>;
	int partId = this->partId;
	this->partId = idRange.min;
	Dimension startDim = getDimension();
	this->partId = idRange.max;
	Dimension endDim = getDimension();
	Dimension rangeDim = Dimension();
	rangeDim.range.min = startDim.range.min;
	rangeDim.range.max = endDim.range.max;
	rangeDim.setLength();
	IntervalSeq *interval = new IntervalSeq(rangeDim.range.min, rangeDim.length, rangeDim.length, 1);
	list->Append(interval);
	this->partId = partId;
	return list;
}

void BlockCountInstr::getIntervalDescForRangeHierarchy(List<Range> *rangeList, List<IntervalSeq*> *descInConstruct) {
	Range idRange = rangeList->Nth(rangeList->NumElements() - 1);
	if (descInConstruct->NumElements() == 0) {
		List<IntervalSeq*> *myIntervalList = getIntervalDescForRange(idRange);
		descInConstruct->AppendAll(myIntervalList);
		delete myIntervalList;
	} else {
		List<IntervalSeq*> *updatedList = new List<IntervalSeq*>;
		int iterationCount = idRange.max - idRange.min + 1;
		int partId = this->partId;
		this->partId = idRange.min;
		int period = getDimension().length;
		this->partId = partId;
		for (int i = 0; i < descInConstruct->NumElements(); i++) {
			IntervalSeq *subInterval = descInConstruct->Nth(i);
			// updating the existing sub-interval to iterate multiple times
			if (subInterval->count == 1) {
				subInterval->count = iterationCount;
				subInterval->period = period;
				updatedList->Append(subInterval);
			// generating new smaller intervals for each iteration of the sub-interval
			} else {
				for (int j = 0; j < subInterval->count; j++) {
					int newBegin = subInterval->begin + subInterval->period * j;
					IntervalSeq *newInterval = new IntervalSeq(newBegin, subInterval->length, period, iterationCount);
					updatedList->Append(newInterval);
				}
			}
		}
		descInConstruct->clear();
		descInConstruct->AppendAll(updatedList);
		delete updatedList;
	}
	rangeList->RemoveAt(rangeList->NumElements() - 1);
	if (prevInstr != NULL) prevInstr->getIntervalDescForRangeHierarchy(rangeList, descInConstruct);
}

//-------------------------------------------------------- Stride --------------------------------------------------------

StrideInstr::StrideInstr(Dimension pd, int id, int ppuCount)
: PartitionInstr("Stride", pd, id, 0, true) {
	int length = pd.length;
	this->partsCount = max(1, min(ppuCount, length));
	this->ppuCount = ppuCount;
}

Dimension StrideInstr::getDimension() {
	int length = parentDim.length;
	int perStrideEntries = length / partsCount;
	int myEntries = perStrideEntries;
	int remainder = length % partsCount;
	if (remainder > partId) {
		myEntries++;
	}
	Dimension partDimension;
	partDimension.range.min = 0;
	partDimension.range.max = myEntries - 1;
	partDimension.length = myEntries;
	return partDimension;
}

List<IntervalSeq*> *StrideInstr::getIntervalDesc() {
	List<IntervalSeq*> *list = new List<IntervalSeq*>;
	int length = parentDim.length;
	int strides = length / partsCount;
	int remaining = length % partsCount;
	if (remaining > partId) strides++;
	int begin = parentDim.range.min + partId;
	IntervalSeq *interval = new IntervalSeq(begin, 1, partsCount, strides);
	list->Append(interval);
	return list;
}

void StrideInstr::getIntervalDesc(List<IntervalSeq*> *descInConstruct) {
	IntervalSeq *myIntervalDesc = getIntervalDesc()->Nth(0);
	if (descInConstruct->NumElements() == 0) {
		descInConstruct->Append(myIntervalDesc);
	} else {
		List<IntervalSeq*> *newList = new List<IntervalSeq*>;
		while (descInConstruct->NumElements() > 0) {
			IntervalSeq *currInterval = descInConstruct->Nth(0);
			newList->AppendAll(myIntervalDesc->transformSubInterval(currInterval));
			descInConstruct->RemoveAt(0);
			delete currInterval;
		}
		descInConstruct->AppendAll(newList);
		while (newList->NumElements() > 0) newList->RemoveAt(0);
		delete newList;
	}
	if (prevInstr != NULL) {
		prevInstr->getIntervalDesc(descInConstruct);
	}
}

int StrideInstr::calculatePartsCount(Dimension dimension, bool updateProperties) {
	int count = max(1, min(ppuCount, dimension.length));
	if (updateProperties) {
		this->parentDim = dimension;
		this->partsCount = count;
	}
	return count;
}

List<IntervalSeq*> *StrideInstr::getIntervalDescForRange(Range idRange) {

	List<IntervalSeq*> *list = new List<IntervalSeq*>;

	if (partsCount == idRange.max - idRange.min + 1) {
		int begin = parentDim.range.min;
		int length = parentDim.length;
		int period = length;
		IntervalSeq *interval = new IntervalSeq(begin, length, period, 1);
		list->Append(interval);
	} else {
		int strides = parentDim.length / partsCount;
		int remaining = parentDim.length % partsCount;
		int spillOverEntries = 0;
		if (remaining > idRange.max) strides++;
		else if (remaining > 0) spillOverEntries = max(0, remaining - idRange.min);

		int begin = parentDim.range.min + idRange.min;
		int length = idRange.max - idRange.min + 1;
		IntervalSeq *mainInterval = new IntervalSeq(begin, length, partsCount, strides);
		list->Append(mainInterval);

		if (spillOverEntries > 0) {
			int spillBegin = begin + partsCount * strides;
			int spillLength = spillOverEntries;
			IntervalSeq *spillInterval = new IntervalSeq(spillBegin, spillLength, spillLength, 1);
			list->Append(spillInterval);
		}
	}

	return list;
}

void StrideInstr::getIntervalDescForRangeHierarchy(List<Range> *rangeList, List<IntervalSeq*> *descInConstruct) {
	Range idRange = rangeList->Nth(rangeList->NumElements() - 1);
	if (descInConstruct->NumElements() == 0) {
		List<IntervalSeq*> *myIntervalList = getIntervalDescForRange(idRange);
		descInConstruct->AppendAll(myIntervalList);
		delete myIntervalList;
	} else {
		List<IntervalSeq*> *newList = new List<IntervalSeq*>;
		int partId = this->partId;
		for (int i = idRange.min; i <= idRange.max; i++) {
			this->partId = i;
			List<IntervalSeq*> *myIntervalList = getIntervalDesc();
			IntervalSeq *myInterval = myIntervalList->Nth(0);
			delete myIntervalList;
			for (int j = 0; j < descInConstruct->NumElements(); j++) {
				IntervalSeq *subSeq = descInConstruct->Nth(j);
				newList->AppendAll(myInterval->transformSubInterval(subSeq));
			}
			delete myInterval;
			delete myIntervalList;
		}
		this->partId = partId;
		while (descInConstruct->NumElements() > 0) {
			IntervalSeq *currSeq = descInConstruct->Nth(0);
			descInConstruct->RemoveAt(0);
			delete currSeq;
		}
		descInConstruct->AppendAll(newList);
		delete newList;
	}
	rangeList->RemoveAt(rangeList->NumElements() - 1);
	if (prevInstr != NULL) prevInstr->getIntervalDescForRangeHierarchy(rangeList, descInConstruct);
}

//----------------------------------------------------- Block Stride -----------------------------------------------------

BlockStrideInstr::BlockStrideInstr(Dimension pd, int id, int ppuCount, int size)
: PartitionInstr("Block-Stride", pd, id, 0, true) {
	this->size = size;
	this->ppuCount = ppuCount;
	int length = pd.length;
	int strides = length / size;
	partsCount = max(1, min(strides, ppuCount));
}

Dimension BlockStrideInstr::getDimension() {
	int strideLength = size * partsCount;
	int strideCount = parentDim.length / strideLength;
	int myEntries = strideCount * size;

	int partialStrideElements = parentDim.length % strideLength;
	int blockCount = partialStrideElements / size;
	int extraEntriesBefore = partialStrideElements;

	if (blockCount > partId) {
		myEntries += size;
		extraEntriesBefore = partId * size;
	} else if (blockCount == partId) {
		myEntries += extraEntriesBefore - partId * size;
		extraEntriesBefore = partId * size;
	}

	Dimension partDimension;
	partDimension.range.min = 0;
	partDimension.range.max = myEntries - 1;
	partDimension.length = myEntries;
	return partDimension;
}

List<IntervalSeq*> *BlockStrideInstr::getIntervalDesc() {

	List<IntervalSeq*> *list = new List<IntervalSeq*>;
	int strideLength = size * partsCount;
	int strideCount = parentDim.length / strideLength;

	int partialStrideElements = parentDim.length % strideLength;
	int extraBlockCount = partialStrideElements / size;

	if (extraBlockCount > partId) strideCount++;

	int begin = parentDim.range.min + size * partId;
	int length = size;
	int count = strideCount;
	int period = strideLength;
	IntervalSeq *iterativeInterval = new IntervalSeq(begin, length, period, count);
	list->Append(iterativeInterval);

	if (extraBlockCount == partId && partialStrideElements % size != 0) {
		int spill = partialStrideElements % size;
		int spillStarts = parentDim.range.min + strideCount * strideLength + extraBlockCount * size;
		IntervalSeq *spillInterval = new IntervalSeq(spillStarts, spill, spill, 1);
		list->Append(spillInterval);
	}

	return list;
}

void BlockStrideInstr::getIntervalDesc(List<IntervalSeq*> *descInConstruct) {
	List<IntervalSeq*> *newList = new List<IntervalSeq*>;
	List<IntervalSeq*> *myIntervalDescList = getIntervalDesc();
	if (descInConstruct->NumElements() == 0) {
		descInConstruct->AppendAll(myIntervalDescList);
	} else {
		IntervalSeq *myIntervalDesc = myIntervalDescList->Nth(0);
		if (myIntervalDescList->NumElements() > 1) {
			myIntervalDesc->increaseCount(1);
		}
		while (descInConstruct->NumElements() > 0) {
			IntervalSeq *currInterval = descInConstruct->Nth(0);
			newList->AppendAll(myIntervalDesc->transformSubInterval(currInterval));
			descInConstruct->RemoveAt(0);
			delete currInterval;
		}
		descInConstruct->AppendAll(newList);
		while (newList->NumElements() > 0) newList->RemoveAt(0);
		delete newList;
	}
	if (prevInstr != NULL) {
		prevInstr->getIntervalDesc(descInConstruct);
	}
}

int BlockStrideInstr::calculatePartsCount(Dimension dimension, bool updateProperties) {
	int strides = dimension.length / size;
	int count = max(1, min(strides, ppuCount));
	if (updateProperties) {
		this->parentDim = dimension;
		this->partsCount = count;
	}
	return count;
}

List<IntervalSeq*> *BlockStrideInstr::getIntervalDescForRange(Range idRange) {

	List<IntervalSeq*> *list = new List<IntervalSeq*>;

	if (partsCount == idRange.max - idRange.min + 1) {
		int begin = parentDim.range.min;
		int length = parentDim.length;
		int period = length;
		IntervalSeq *interval = new IntervalSeq(begin, length, period, 1);
		list->Append(interval);
	} else {
		int strideLength = size * partsCount;
		int strideCount = parentDim.length / strideLength;
		int partialStrideElements = parentDim.length % strideLength;
		int extraBlockCount = partialStrideElements / size;
		int spillOver = 0;
		if (extraBlockCount > idRange.max) strideCount++;
		else spillOver = max(0, partialStrideElements - size * idRange.min);

		int begin = parentDim.range.min + size * idRange.min;
		int length = size * (idRange.max - idRange.min + 1);
		int count = strideCount;
		int period = strideLength;
		IntervalSeq *iterativeInterval = new IntervalSeq(begin, length, period, count);
		list->Append(iterativeInterval);

		if (spillOver > 0) {
			int spillStarts = begin + strideCount * strideLength;
			IntervalSeq *spillInterval = new IntervalSeq(spillStarts, spillOver, spillOver, 1);
			list->Append(spillInterval);
		}
	}

	return list;
}

void BlockStrideInstr::getIntervalDescForRangeHierarchy(List<Range> *rangeList, List<IntervalSeq*> *descInConstruct) {
	Range idRange = rangeList->Nth(rangeList->NumElements() - 1);
	if (descInConstruct->NumElements() == 0) {
		List<IntervalSeq*> *myIntervalList = getIntervalDescForRange(idRange);
		descInConstruct->AppendAll(myIntervalList);
		delete myIntervalList;
	} else {
		List<IntervalSeq*> *newList = new List<IntervalSeq*>;
		int partId = this->partId;
		for (int i = idRange.min; i <= idRange.max; i++) {
			this->partId = i;
			List<IntervalSeq*> *myIntervalList = getIntervalDesc();
			IntervalSeq *myInterval = myIntervalList->Nth(0);
			if (myIntervalList->NumElements() > 1) {
				myInterval->increaseCount(1);
			}
			delete myIntervalList;
			for (int j = 0; j < descInConstruct->NumElements(); j++) {
				IntervalSeq *subSeq = descInConstruct->Nth(j);
				newList->AppendAll(myInterval->transformSubInterval(subSeq));
			}
			delete myInterval;
			delete myIntervalList;
		}
		this->partId = partId;
		while (descInConstruct->NumElements() > 0) {
			IntervalSeq *currSeq = descInConstruct->Nth(0);
			descInConstruct->RemoveAt(0);
			delete currSeq;
		}
		descInConstruct->AppendAll(newList);
		delete newList;
	}
	rangeList->RemoveAt(rangeList->NumElements() - 1);
	if (prevInstr != NULL) prevInstr->getIntervalDescForRangeHierarchy(rangeList, descInConstruct);
}

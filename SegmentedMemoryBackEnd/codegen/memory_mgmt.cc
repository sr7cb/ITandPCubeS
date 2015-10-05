#include "memory_mgmt.h"
#include "../semantics/task_space.h"
#include "../memory-management/allocation.h"
#include "../memory-management/part_generation.h"
#include "../utils/list.h"
#include "../utils/hashtable.h"
#include "../utils/decorator_utils.h"
#include "../utils/string_utils.h"
#include "../utils/code_constant.h"
#include "../syntax/ast.h"
#include "../syntax/ast_expr.h"

#include <fstream>
#include <sstream>
#include <cstdlib>
#include <deque>

void genRoutineForDataPartConfig(std::ofstream &headerFile,
                std::ofstream &programFile,
                const char *initials,
                Space *lps,
                ArrayDataStructure *array) {
	
	std::ostringstream functionHeader;
	functionHeader << "get" << array->getName() << "ConfigForSpace" << lps->getName();
	functionHeader << "(ArrayMetadata *metadata" << paramSeparator;
	functionHeader << "\n" << doubleIndent;
	functionHeader << initials << "Partition partition" << paramSeparator;
	functionHeader << "\n" << doubleIndent;
	functionHeader << "int ppuCount" << paramSeparator;
	functionHeader << "\n" << doubleIndent;
	functionHeader << "Hashtable<DataPartitionConfig*> *configMap)";

	headerFile << "DataPartitionConfig *" << functionHeader.str() << stmtSeparator;
	programFile << "DataPartitionConfig *" << string_utils::toLower(initials);
	programFile << "::" << functionHeader.str() << " {\n";

	// create a list of data dimension configs that will be used to create the final partition config
	// object
	int dimensionCount = array->getDimensionality();
	programFile << indent << "List<DimPartitionConfig*> *dimensionConfigs";
	programFile << " = new List<DimPartitionConfig*>" << stmtSeparator;

	for (int i = 0; i < dimensionCount; i++) {
		PartitionFunctionConfig *partitionConfig = array->getPartitionSpecForDimension(i + 1);
		// if the partition config allong a dimension is NULL then the structure is unpartitioned
		// along that dimension and we should add a replication config here
		if (partitionConfig == NULL) {
			programFile << indent << "dimensionConfigs->Append(";
			programFile << "new ReplicationConfig(metadata->" << array->getName();
			programFile << "Dims[" << i << "]))" << stmtSeparator;
		} else {
			// get the name of the configuration class for the partition function been used
			const char *configClassName = partitionConfig->getDimensionConfigClassName();
			// get dimension configuration
			DataDimensionConfig *partitionArgs = partitionConfig->getArgsForDimension(i + 1);
			// check if the function supports padding
			bool paddingSupported = partitionConfig->doesSupportGhostRegion();
			// if padding is supported then we need a two elements array to hold the padding
			// configurations for front and back of each partition
			if (paddingSupported) {
				programFile << indent << "int *dim" << i << "Paddings = new int[2]";
				programFile << stmtSeparator;
				programFile << indent << "dim" << i << "Paddings[0] = ";
				Node *frontPadding = partitionArgs->getFrontPaddingArg();
				if (frontPadding == NULL) {
					programFile << "0";
				} else {
					programFile << DataDimensionConfig::getArgumentString(
							frontPadding, "partition.");
				}
				programFile << stmtSeparator;		
				programFile << indent << "dim" << i << "Paddings[1] = ";
				Node *rearPadding = partitionArgs->getBackPaddingArg();
				if (rearPadding == NULL) {
					programFile << "0";
				} else {
					programFile << DataDimensionConfig::getArgumentString(
							rearPadding, "partition.");
				}
				programFile << stmtSeparator;		
			}
			// check if there is any partitioning argument needed by the function
			Node *dividingParam = partitionArgs->getDividingArg();
			bool hasParameters = dividingParam != NULL;
			// If the partition function supports parameters then configuration class instance
			// should get values of those parameters passed in a list. Note that currently we
			// have only single or no argument partition functions so code generation logic is
			// designed accordingly. In future, this restriction should be lifted and changes
			// should be made in semantics and code-generation modules to reflect that change.
			if (hasParameters) {
				programFile << indent << "int *dim" << i << "Arguments = new int";
				programFile << stmtSeparator << indent << "dim" << i << "Arguments[0] = ";
				programFile << DataDimensionConfig::getArgumentString(dividingParam, 
					"partition.");
				programFile << stmtSeparator;
			}

			// determines with what dimension of the LPS the current dimension of the array has
			// been aligned to
			CoordinateSystem *coordSys = lps->getCoordinateSystem();
			int matchingDim = i;
			int spaceDimensions = lps->getDimensionCount();
			int j = 0;
			for (; j < spaceDimensions; j++) {
				Coordinate *coordinate = coordSys->getCoordinate(j + 1);
				Token *token = coordinate->getTokenForDataStructure(array->getName());
				if (token != NULL && !token->isWildcard()) {
					if (token->getDimensionId() == i + 1) break;
				}
			}
			matchingDim = j;
			
			// create the dimension configuration object with all information found and add it in
			// the list
			programFile << indent << "dimensionConfigs->Append(";
			programFile << "new " << configClassName << "(";
			programFile << "metadata->" << array->getName() << "Dims[" << i << "]";
			programFile << paramSeparator << '\n' << indent << doubleIndent;
			if (hasParameters) {
				programFile << "dim" << i << "Arguments";
				programFile << paramSeparator;
			}
			if (paddingSupported) {
				programFile << "dim" << i << "Paddings";
				programFile << paramSeparator;
			}
			programFile << "ppuCount";
			programFile << paramSeparator << matchingDim;
			programFile << "))" << stmtSeparator;
			
			// reclaim the storage for padding configuration if applicable
			if (paddingSupported) {
				programFile << indent << "delete[] dim" << i << "Paddings" << stmtSeparator;
			}
		}
	}

	// create the partition config object
	programFile << indent << "DataPartitionConfig *config =  new DataPartitionConfig(";
	programFile << dimensionCount << paramSeparator << "dimensionConfigs)" << stmtSeparator;

	// check if there is any ancestor partition in higher LPSes for this data; if YES then set the parent
	// config reference in the newly created config
	DataStructure *source = array->getSource();
	if (source != NULL && !source->getSpace()->isRoot()) {
		Space *parentLpsForArray = source->getSpace();
		Space *parentLps = lps->getParent();
		int jumps = 1;
		while (parentLps != parentLpsForArray) {
			parentLps = parentLps->getParent();
			jumps++;
		}
		programFile << indent << "config->setParent(configMap->Lookup(\"";
		programFile << array->getName() << "Space" << parentLps->getName() << "Config";
		programFile << "\")" << paramSeparator << jumps << ")" << stmtSeparator;
	}

	// configure a dimension-config vector reference to be used to generate a part container for holding 
	// all data-structure parts for the underlying data structure for current LPS for the segment
	programFile << indent << "config->configureDimensionOrder()" << stmtSeparator; 

	// finally, set the integer LPS identifier in the configuration that will be needed to traverse the
	// part-distribution-tree that holds partition hierarchy information of a data structure's parts for
	// different LPSes.
	programFile << indent << "config->setLpsId(Space_" << lps->getName() << ")" << stmtSeparator; 

	programFile << indent << "return config" << stmtSeparator;
	programFile << "}\n";
}

void genRoutinesForTaskPartitionConfigs(const char *headerFileName,
                const char *programFileName,
                const char *initials,
                PartitionHierarchy *hierarchy) {

	std::cout << "Generating routines to construct data partition configuration\n";
        
	std::ofstream programFile, headerFile;
        programFile.open (programFileName, std::ofstream::out | std::ofstream::app);
        headerFile.open (headerFileName, std::ofstream::out | std::ofstream::app);
        if (!programFile.is_open() || !headerFile.is_open()) {
                std::cout << "Unable to open header/program file";
                std::exit(EXIT_FAILURE);
        }
	const char *header = "functions for generating partition configuration objects for data structures";
	decorator::writeSectionHeader(headerFile, header);
	decorator::writeSectionHeader(programFile, header);

	// generate a header for the function that will create dimension configuration of data structures in
	// different LPSes and store them in a map
	std::ostringstream functionHeader;
	functionHeader << "getDataPartitionConfigMap("; 
	functionHeader << "ArrayMetadata *metadata" << paramSeparator;
	functionHeader << "\n" << doubleIndent;
	functionHeader << initials << "Partition partition" << paramSeparator;
	functionHeader << "int *ppuCounts)";

	// first create the map in the function body
	std::ostringstream functionBody;
	functionBody << indent << "Hashtable<DataPartitionConfig*> *configMap = ";
	functionBody << "new Hashtable<DataPartitionConfig*>" << stmtSeparator;

	// iterate all LPSes except the root and create functions for partition configs for relevent arrays
	Space *root = hierarchy->getRootSpace();
	std::deque<Space*> lpsQueue;
	List<Space*> *children = root->getChildrenSpaces();
	for (int i = 0; i < children->NumElements(); i++) {
		lpsQueue.push_back(children->Nth(i));
	}
        while (!lpsQueue.empty()) {
                Space *lps = lpsQueue.front();
                lpsQueue.pop_front();
                children = lps->getChildrenSpaces();
                for (int i = 0; i < children->NumElements(); i++) {
                        lpsQueue.push_back(children->Nth(i));
                }
                if (lps->getSubpartition() != NULL) lpsQueue.push_back(lps->getSubpartition());
		
		int generationCount = 0;
		List<const char*> *structureList = lps->getLocalDataStructureNames();
		for (int i = 0; i < structureList->NumElements(); i++) {
			DataStructure *structure = lps->getLocalStructure(structureList->Nth(i));
			ArrayDataStructure *array = dynamic_cast<ArrayDataStructure*>(structure);
			if (array == NULL) continue;

			if (generationCount == 0) {
				std::ostringstream message;
				message << "Space " << lps->getName();
				const char *c_message = message.str().c_str();
				decorator::writeSubsectionHeader(headerFile, c_message);
				decorator::writeSubsectionHeader(programFile, c_message);
			}
			programFile << std::endl;
			genRoutineForDataPartConfig(headerFile, programFile, initials, lps, array);
			generationCount++;
			
			// add statements in the accumulator function for calling the generated method and
			// storing its result in the map
			std::ostringstream configName;
			configName << array->getName() << "Space" << lps->getName() << "Config";
			std::ostringstream configFunction;
			configFunction << "get" << array->getName() << "ConfigForSpace" << lps->getName();
			configFunction << "(metadata" << paramSeparator; 
			configFunction << "\n" << doubleIndent;
			configFunction << "partition" << paramSeparator;
			configFunction << "ppuCounts[Space_" << lps->getName() << "]" << paramSeparator;
			configFunction << "configMap)";
			functionBody << indent << "DataPartitionConfig *" << configName.str();
			functionBody << " = " << configFunction.str() << stmtSeparator;
			functionBody << indent << "configMap->Enter(\"" << configName.str() << "\"";
			functionBody << paramSeparator << configName.str() << ")" << stmtSeparator;			
		}
	}

	// write the accumulator function in both files
	const char *message = "Partition Configuration Accumulator";
	decorator::writeSubsectionHeader(headerFile, message);
	headerFile << "Hashtable<DataPartitionConfig*> *" << functionHeader.str() << stmtSeparator;
	headerFile << "\n";
	decorator::writeSubsectionHeader(programFile, message);
	programFile << "\n";
	programFile << "Hashtable<DataPartitionConfig*> *";
	programFile << string_utils::toLower(initials) << "::" << functionHeader.str();
	functionBody << indent << "return configMap" << stmtSeparator;
	programFile << " {\n" << functionBody.str() << "}\n";

	headerFile.close();
	programFile.close();
}
	
void genRoutineForLpsContent(std::ofstream &headerFile, 
		std::ofstream &programFile, const char *initials, Space *lps, Space *rootLps) {
	
	std::ostringstream functionHeader;
	functionHeader << "genSpace" << lps->getName() << "Content(";
	functionHeader << "List<ThreadState*> *threads" << paramSeparator;
	functionHeader << "ArrayMetadata *metadata" << paramSeparator;
	functionHeader << '\n' << doubleIndent;
	functionHeader << initials << "Partition partition" << paramSeparator;
	functionHeader << '\n' << doubleIndent;
	functionHeader << "Hashtable<DataPartitionConfig*> *partConfigMap)";

	headerFile << "LpsContent *" << functionHeader.str() << stmtSeparator;
	programFile << "LpsContent *" << string_utils::toLower(initials) << "::";
	programFile << functionHeader.str() << " {\n";

	// write a checking for non-empty thread list; if the list is empty then return NULL
	programFile << std::endl << indent;
	programFile << "if(threads->NumElements() == 0) return NULL" << stmtSeparator;

	// create an LPS content object
	const char *lpsName = lps->getName();
	programFile << indent << "LpsContent *space" << lpsName << "Content = new LpsContent(";
	programFile << "Space_" << lpsName << ")" << stmtSeparator;

	// iterate over the data structures of this LPS and create data items for those that are flagged to be 
	// allocated in it.
	List<const char*> *structureList = lps->getLocalDataStructureNames();
	for (int i = 0; i < structureList->NumElements(); i++) {
		DataStructure *structure = lps->getLocalStructure(structureList->Nth(i));
		if (!structure->getUsageStat()->isAllocated()) continue;
	
		programFile << std::endl;
		Type *type = structure->getType();
		int epochCount = structure->getVersionCount() + 1;
		const char *varName = structure->getName();
	
		ArrayDataStructure *array = dynamic_cast<ArrayDataStructure*>(structure);
		if (array == NULL) {
			programFile << indent << "ScalarDataItems *" << varName << " = ";
			programFile << "new ScalarDataItems(\"" << varName << "\"" << paramSeparator;
			programFile << epochCount << ")" << stmtSeparator;
			programFile << indent<< "ScalarDataItems::allocate";
			programFile << "<" << type->getCType() << ">(" << varName << ")" << stmtSeparator;
		} else {
			int dimensionCount = array->getDimensionality();
			programFile << indent << "DataItems *" << varName << " = new DataItems(";
			programFile << '"' << varName << '"' << paramSeparator;
			programFile << dimensionCount << paramSeparator;
			programFile << epochCount << ")" << stmtSeparator;

			// retrieve the partition config object
			programFile << indent << "DataPartitionConfig *" << varName << "Config = ";
			programFile << "partConfigMap->Lookup(";
			programFile << '"' << varName << "Space" << lpsName << "Config" << '"' << ")";
			programFile << stmtSeparator;
			programFile << indent << varName << "->setPartitionConfig(" << varName;
			programFile << "Config" << ")" << stmtSeparator;

			// create an uninitialized data-parts-list object
			programFile << indent << "DataPartsList *" << varName << "Parts = ";
			programFile << varName << "Config->generatePartList(" << epochCount;
			programFile << ")" << stmtSeparator;
			programFile << indent << varName << "->setPartsList(";
			programFile << varName << "Parts)" << stmtSeparator; 

			// retrieve the dimension order instance for data parts from the configuration object
			programFile << indent << "std::vector<DimConfig> " << varName << "DimOrder = *(";
			programFile << varName << "Config->getDimensionOrder())" << stmtSeparator;

			// create a part-container that will aid the data-parts-list in parts searching and 
			// management
			programFile << indent << "PartIdContainer *" << varName << "Container = NULL";
			programFile << stmtSeparator;
			programFile << indent << "if (" << varName << "DimOrder.size() == 1) " << varName;
			programFile << "Container = new PartContainer(";
			programFile << varName << "DimOrder[0])" << stmtSeparator; 
			programFile << indent << "else " << varName;
			programFile << "Container = new PartListContainer(";
			programFile << varName << "DimOrder[0])" << stmtSeparator; 
		
			// create a blank part Id template reference
			programFile << indent << "List<int*> *" << varName << "PartId = ";
			programFile << varName << "Config->generatePartIdTemplate()" << stmtSeparator;
		}

		// add the data item in the LPS content
		programFile << indent << "space" << lpsName << "Content->addDataItems(";
		programFile << '"' << varName << '"' << paramSeparator;
		programFile << varName << ")" << stmtSeparator;
	}

	// iterate over the threads of the segment and try to initialize the parts of each allocated data structures
	programFile << std::endl << indent << "for (int i = 0; i < threads->NumElements(); i++) {\n";
	programFile << doubleIndent << "ThreadState *thread = threads->Nth(i)" << stmtSeparator;
	programFile << doubleIndent << "int lpuId = INVALID_ID" << stmtSeparator;
	programFile << doubleIndent << "while((lpuId = thread->getNextLpuId(";
	programFile << "Space_" << lpsName << paramSeparator;
	programFile << "Space_" << rootLps->getName() << paramSeparator;
	programFile << "lpuId)) != INVALID_ID) {\n";
	programFile << tripleIndent << "List<int*> *lpuIdChain = thread->getLpuIdChainWithoutCopy(";
	programFile << std::endl << tripleIndent << doubleIndent;
	programFile << "Space_" << lpsName << paramSeparator;
	programFile << "Space_" << rootLps->getName() << ")" << stmtSeparator;
	
	// this time consider only arrays that are allocated within this LPS as partitioning is done for arrays only
	for (int i = 0; i < structureList->NumElements(); i++) {
		DataStructure *structure = lps->getLocalStructure(structureList->Nth(i));
		if (!structure->getUsageStat()->isAllocated()) continue;
		const char *varName = structure->getName();
		ArrayDataStructure *array = dynamic_cast<ArrayDataStructure*>(structure);
		if (array == NULL) continue;
		
		programFile << tripleIndent << varName << "Config->generatePartId(lpuIdChain" << paramSeparator;
		programFile << varName << "PartId)"  << stmtSeparator;
		programFile << tripleIndent << varName << "Container->insertPartId(";
		programFile << varName << "PartId" << paramSeparator;
		programFile << array->getDimensionality() << paramSeparator;
		programFile << varName << "DimOrder)" << stmtSeparator;
	}

	programFile << doubleIndent << "}\n";	
	programFile << indent << "}\n";

	// allocate memory for all data parts in their corresponding data parts list
	for (int i = 0; i < structureList->NumElements(); i++) {
		DataStructure *structure = lps->getLocalStructure(structureList->Nth(i));
		if (!structure->getUsageStat()->isAllocated()) continue;
		const char *varName = structure->getName();
		ArrayDataStructure *array = dynamic_cast<ArrayDataStructure*>(structure);
		if (array == NULL) continue;
		programFile << indent << "DataPartsList::allocate ";
		Type *type = structure->getType();
		ArrayType *arrayType = reinterpret_cast<ArrayType*>(type);
		programFile << "<" << arrayType->getTerminalElementType()->getCType() << ">(";
		programFile << varName << "Parts" << paramSeparator;
		programFile << varName << "Config" << paramSeparator;
		programFile << varName << "Container)" << stmtSeparator;
	}	
	
	programFile << std::endl << indent << "return space" << lpsName << "Content" << stmtSeparator;		
	programFile << "}\n";	
}

void genTaskMemoryConfigRoutine(const char *headerFileName,
                const char *programFileName,
                const char *initials,
                PartitionHierarchy *hierarchy) {
	
	std::cout << "Generating routines to construct and manage task's memory allocations\n";

        std::ofstream programFile, headerFile;
        programFile.open (programFileName, std::ofstream::out | std::ofstream::app);
        headerFile.open (headerFileName, std::ofstream::out | std::ofstream::app);
        if (!programFile.is_open() || !headerFile.is_open()) {
                std::cout << "Unable to open header/program file";
                std::exit(EXIT_FAILURE);
        }
	const char *header = "functions for generating memory blocks for data parts of various LPUs";
	decorator::writeSectionHeader(headerFile, header);
	headerFile << std::endl;
	decorator::writeSectionHeader(programFile, header);

	// generate the function header for the routine that will generate a task-data object tracking all
	// data pieces of different structures needed within a PPU for a task's computations
	std::ostringstream functionHeader;	
	functionHeader << "initializeTaskData("; 
	functionHeader << "List<ThreadState*> *threads" << paramSeparator;
	functionHeader << "ArrayMetadata *metadata" << paramSeparator;
	functionHeader << "\n" << doubleIndent;
	functionHeader << initials << "Partition partition" << paramSeparator;
	functionHeader << "int *ppuCounts)";

	// generate an instance of data partition configuration map first within the function body
	std::ostringstream functionBody;
	functionBody << indent << "Hashtable<DataPartitionConfig*> *configMap = ";
	functionBody << '\n' << indent << doubleIndent;
	functionBody << "getDataPartitionConfigMap(metadata" << paramSeparator << "partition";
	functionBody << paramSeparator << "ppuCounts)" << stmtSeparator;

	// create a task data object
	functionBody << indent << "TaskData *taskData = new TaskData()" << stmtSeparator;
	
	// iterate all LPSes and create LPS Content instance in each that has any data structure to allocate 
	Space *root = hierarchy->getRootSpace();
	std::deque<Space*> lpsQueue;
	lpsQueue.push_back(root);
        while (!lpsQueue.empty()) {
                Space *lps = lpsQueue.front();
                lpsQueue.pop_front();
                List<Space*> *children = lps->getChildrenSpaces();
                for (int i = 0; i < children->NumElements(); i++) {
                        lpsQueue.push_back(children->Nth(i));
                }
                if (lps->getSubpartition() != NULL) lpsQueue.push_back(lps->getSubpartition());
		if (!lps->allocateStructures()) continue;
		
		programFile << std::endl;
		genRoutineForLpsContent(headerFile, programFile, initials, lps, root);

		const char *lpsName = lps->getName();
		functionBody << indent << "LpsContent *space" << lpsName << "Content = ";
		functionBody << "genSpace" << lpsName << "Content(threads" << paramSeparator;
		functionBody << "metadata" << paramSeparator << "partition" << paramSeparator;
		functionBody << "configMap)" << stmtSeparator;
		functionBody << indent << "taskData->addLpsContent(\"" << lpsName << '"';
		functionBody << paramSeparator << "space" << lpsName << "Content)" << stmtSeparator;
	}
	
	// write the task data initializer function in both files
	const char *message = "Task Data Initializer";
	decorator::writeSubsectionHeader(headerFile, message);
	headerFile << "TaskData *" << functionHeader.str() << stmtSeparator << "\n";
	decorator::writeSubsectionHeader(programFile, message);
	programFile << "\n" << "TaskData *";
	programFile << string_utils::toLower(initials) << "::" << functionHeader.str();
	functionBody << indent << "return taskData" << stmtSeparator;
	programFile << " {\n" << functionBody.str() << "}\n";
	
	headerFile.close();
	programFile.close();
}

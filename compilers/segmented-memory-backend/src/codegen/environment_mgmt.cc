#include "environment_mgmt.h"
#include "../syntax/ast_task.h"
#include "../syntax/ast_type.h"
#include "../semantics/task_space.h"
#include "../static-analysis/task_env_stat.h"
#include "../utils/list.h"
#include "../utils/hashtable.h"
#include "../utils/decorator_utils.h"
#include "../utils/string_utils.h"
#include "../utils/code_constant.h"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <deque>	

void generateTaskEnvironmentClass(TaskDef *taskDef, const char *initials, 
		const char *headerFileName, 
		const char *programFileName) {
	
	std::cout << "Generating structures and routines for environment management\n";

        std::ofstream programFile, headerFile;
        programFile.open (programFileName, std::ofstream::out | std::ofstream::app);
        headerFile.open (headerFileName, std::ofstream::out | std::ofstream::app);
        if (!programFile.is_open() || !headerFile.is_open()) {
                std::cout << "Unable to open header/program file";
                std::exit(EXIT_FAILURE);
        }
        const char *header = "Task Environment Management Structures and Functions";
        decorator::writeSectionHeader(headerFile, header);
        decorator::writeSectionHeader(programFile, header);
	const char *message = "Task Environment Implementation Class";
	decorator::writeSubsectionHeader(headerFile, message);

	// declare the extension class for task environment in the header file
	headerFile << '\n' << "class TaskEnvironmentImpl : public TaskEnvironment {\n";
	headerFile << "  public:\n";
	headerFile << indent << "TaskEnvironmentImpl() : TaskEnvironment() {}\n";
	headerFile << indent << "void prepareItemMaps()" << stmtSeparator;
	headerFile << indent << "void setDefaultTaskCompletionInstrs()" << stmtSeparator;
	headerFile << "}" << stmtSeparator;

	// three functions need to be implemented by the task specific environment subclass; call other functions to
	// generate their definitions in the program file
	generateFnForItemsMapPreparation(taskDef, initials, programFile);
	generateFnForTaskCompletionInstrs(taskDef, initials, programFile);

	headerFile.close();
	programFile.close();
}

void generateFnForItemsMapPreparation(TaskDef *taskDef, const char *initials, std::ofstream &programFile) {
	
	Space *rootLps = taskDef->getPartitionHierarchy()->getRootSpace();
	const char *message = "Task Environment Function Implementations";
	decorator::writeSubsectionHeader(programFile, message);

	programFile << '\n';
	programFile << "void " << initials << "::" << "TaskEnvironmentImpl::prepareItemMaps() {\n";

	List<EnvironmentLink*> *envLinkList = taskDef->getEnvironmentLinks();
	for (int i = 0; i < envLinkList->NumElements(); i++) {
		
		EnvironmentLink *link = envLinkList->Nth(i);
		const char *varName = link->getVariable()->getName();
		DataStructure *structure = rootLps->getStructure(varName);
		ArrayDataStructure *array = dynamic_cast<ArrayDataStructure*>(structure);
		StaticArrayType *staticArray = dynamic_cast<StaticArrayType*>(structure->getType());
		
		// at this moment, we are doing environment management for arrays only
		if (array == NULL || staticArray != NULL) continue;

		programFile << '\n';
		ArrayType *type = (ArrayType*) array->getType();
		int dimensionality = type->getDimensions();
		int linkId = i;

		programFile << indent << "EnvironmentLinkKey *key" << linkId << " = new EnvironmentLinkKey(";
		programFile << "\"" << varName << "\"" << paramSeparator;
		programFile << linkId << ")" << stmtSeparator;

		programFile << indent << "TaskItem *item" << linkId << " = new TaskItem(";
		programFile << "key" << linkId << paramSeparator;

		LinkageType linkageType = link->getMode();
		if (linkageType == TypeLink) {
			programFile << "IN_OUT";
		} else if (linkageType == TypeCreateIfNotLinked) {
			programFile << "OPTIONAL_IN_OUT";
		} else {
			programFile << "OUT";
		}
		programFile << paramSeparator;
		programFile << dimensionality << paramSeparator;
		Type *elementType = type->getTerminalElementType();
		programFile << "sizeof(" << elementType->getCType() << "))" << stmtSeparator;
		
		programFile << indent << "envItems->Enter(\"" << varName << "\"" << paramSeparator;
		programFile << "item" << linkId << ")" << stmtSeparator;
	}
	
	programFile << "}\n";
}

void generateFnForTaskCompletionInstrs(TaskDef *taskDef, const char *initials, std::ofstream &programFile) {
	
	Space *rootLps = taskDef->getPartitionHierarchy()->getRootSpace();
	TaskEnvStat *taskEnvStat = taskDef->getAfterExecutionEnvStat();

	programFile << '\n';
	programFile << "void " << initials << "::" << "TaskEnvironmentImpl::setDefaultTaskCompletionInstrs() {\n";

	List<EnvironmentLink*> *envLinkList = taskDef->getEnvironmentLinks();
	for (int i = 0; i < envLinkList->NumElements(); i++) {
		
		EnvironmentLink *link = envLinkList->Nth(i);
		const char *varName = link->getVariable()->getName();
		DataStructure *structure = rootLps->getStructure(varName);
		ArrayDataStructure *array = dynamic_cast<ArrayDataStructure*>(structure);
		StaticArrayType *staticArray = dynamic_cast<StaticArrayType*>(structure->getType());
	
		// at this moment, we are doing environment management for arrays only
		if (array == NULL || staticArray != NULL) continue;

		// if the variable's content has not been accessed at all then there is no environmental update to do for it
		EnvVarStat *varEnvStat = taskEnvStat->getVariableStat(varName);
		if (varEnvStat == NULL) continue;

		// if the variable has been modified then other versions of the data it refers to should be flagged as stale
		if (varEnvStat->isUpdated()) {
			programFile << '\n';
			programFile << indent << "TaskItem *" << varName << "Item = envItems->Lookup(\"";
			programFile << varName << "\")" << stmtSeparator;
			programFile << indent << "ChangeNotifyInstruction *instr" << i << " = new ";
			programFile << "ChangeNotifyInstruction(" << varName << "Item)" << stmtSeparator;
			programFile << indent << "addEndEnvInstruction(instr" << i << ")" << stmtSeparator;
		}
	}
	
	programFile << "}\n";
}

void generateFnToInitEnvLinksFromEnvironment(TaskDef *taskDef, const char *initials,
                const char *headerFileName,
                const char *programFileName) {
	
        std::ofstream programFile, headerFile;
        programFile.open (programFileName, std::ofstream::out | std::ofstream::app);
        headerFile.open (headerFileName, std::ofstream::out | std::ofstream::app);
        if (!programFile.is_open() || !headerFile.is_open()) {
                std::cout << "Unable to open header/program file";
                std::exit(EXIT_FAILURE);
        }
	Space *rootLps = taskDef->getPartitionHierarchy()->getRootSpace();
	const char *message = "Environmental Links Object Generator";
	decorator::writeSubsectionHeader(headerFile, message);
	decorator::writeSubsectionHeader(programFile, message);
	headerFile << '\n';
	programFile << '\n';

	// generate function header
	std::ostringstream fnHeader;
	programFile << "EnvironmentLinks " << initials << "::";
        headerFile << "EnvironmentLinks ";
        fnHeader << "initiateEnvLinks(TaskEnvironment *environment)";
	programFile << fnHeader.str();
        headerFile << fnHeader.str() << stmtSeparator;

        // open function definition
        programFile << " {\n\n";

	// declare a local environment link instance that will be returned by the generated function at the end
	programFile << indent << "EnvironmentLinks links" << stmtSeparator;
	
	List<EnvironmentLink*> *envLinkList = taskDef->getEnvironmentLinks();
	for (int i = 0; i < envLinkList->NumElements(); i++) {
		
		EnvironmentLink *link = envLinkList->Nth(i);
		
		// if the link is for an out variable then it is not a part of the environment links
		if (!link->isExternal()) continue;

		const char *varName = link->getVariable()->getName();
		DataStructure *structure = rootLps->getStructure(varName);
		ArrayDataStructure *array = dynamic_cast<ArrayDataStructure*>(structure);
		StaticArrayType *staticArray = dynamic_cast<StaticArrayType*>(structure->getType());
		
		// at this moment, we are doing environment management for dynamic arrays only
		if (array == NULL || staticArray != NULL) continue;
	
		// retrieve the item corresponding to the environmental variable from the task environment object
		programFile << '\n';
		programFile << indent << "TaskItem *" << varName << "Item = environment->getItem(\"";
		programFile << varName << "\")" << stmtSeparator;
		
		// copy dimension information into the environment link property 
		int dimensions = array->getDimensionality();
		for (int d = 0; d < dimensions; d++) {
			programFile << indent;
			programFile << "links." << varName;
			programFile << "Dims[" << d << "] = ";
			programFile << varName << "Item->getDimension(" << d << ")" << stmtSeparator;
		}
	}

	programFile << '\n' << indent << "return links" << stmtSeparator;
	programFile << "}\n";

	headerFile.close();
	programFile.close();
}

void generateFnToPreconfigureLpsAllocations(TaskDef *taskDef, const char *initials,
                const char *headerFileName,
                const char *programFileName) {
        
	std::ofstream programFile, headerFile;
        programFile.open (programFileName, std::ofstream::out | std::ofstream::app);
        headerFile.open (headerFileName, std::ofstream::out | std::ofstream::app);
        if (!programFile.is_open() || !headerFile.is_open()) {
                std::cout << "Unable to open header/program file";
                std::exit(EXIT_FAILURE);
        }
	Space *rootLps = taskDef->getPartitionHierarchy()->getRootSpace();
	const char *message = "LPS Allocation Preconfigurers";
	decorator::writeSubsectionHeader(headerFile, message);
	decorator::writeSubsectionHeader(programFile, message);
	headerFile << '\n';
	programFile << '\n';

	// generate function header
	std::ostringstream fnHeader;
	programFile << "void " << initials << "::";
        headerFile << "void ";
        fnHeader << "preconfigureLpsAllocationsInEnv(TaskEnvironment *environment";
	fnHeader << paramSeparator << '\n' << doubleIndent << "ArrayMetadata *metadata";
	fnHeader << paramSeparator << '\n' << doubleIndent;
	fnHeader << "Hashtable<DataPartitionConfig*> *partConfigMap)";
	programFile << fnHeader.str();
        headerFile << fnHeader.str() << stmtSeparator;

        // open function definition
        programFile << " {\n\n";

	List<EnvironmentLink*> *envLinkList = taskDef->getEnvironmentLinks();
	for (int i = 0; i < envLinkList->NumElements(); i++) {
		
		EnvironmentLink *link = envLinkList->Nth(i);
		const char *varName = link->getVariable()->getName();
		DataStructure *structure = rootLps->getStructure(varName);
		ArrayDataStructure *array = dynamic_cast<ArrayDataStructure*>(structure);
		StaticArrayType *staticArray = dynamic_cast<StaticArrayType*>(structure->getType());
	
		// at this moment, we are doing environment management for arrays only
		if (array == NULL || staticArray != NULL) continue;

		// retrieve the task item
		programFile << '\n';
		programFile << indent << "TaskItem *" << varName << "Item = environment->getItem(\"";
		programFile << varName << "\")" << stmtSeparator;

		// copy dimension ranges and lengths from array metadata to the task item
		int dimensions = array->getDimensionality();
		for (int d = 0; d < dimensions; d++) {
			programFile << indent;
			programFile << varName << "Item->setDimension(" << d << paramSeparator;
			programFile << "metadata->" << varName << "Dims[" << d << "])" << stmtSeparator;
		}
		
		// go over the partition hierarchy and check the LPSes where the array has been allocated
		std::deque<Space*> lpsQueue;
		lpsQueue.push_back(rootLps);
		while (!lpsQueue.empty()) {
			Space *lps = lpsQueue.front();
			lpsQueue.pop_front();
			List<Space*> *children = lps->getChildrenSpaces();
			for (int i = 0; i < children->NumElements(); i++) {
                        	lpsQueue.push_back(children->Nth(i));
                	}
                	if (lps->getSubpartition() != NULL) lpsQueue.push_back(lps->getSubpartition());
			if (!lps->allocateStructure(varName)) continue;

			// for each LPS that allocates the structure, configure an LPS allocation in the task item
			const char *lpsName = lps->getName();
			std::ostringstream allocationName; 
			allocationName << varName << "InSpace" << lpsName;
			std::ostringstream configName;
			configName << varName << "Space" << lpsName << "Config";
			programFile << indent << "DataPartitionConfig *" << configName.str();
			programFile << " = partConfigMap->Lookup(\"" << configName.str() << "\")" << stmtSeparator;
			programFile << indent << "LpsAllocation *" << allocationName.str() << " = ";
			programFile << varName << "Item->getLpsAllocation(\"" << lpsName << "\")" << stmtSeparator;
			programFile << indent << "if (" << allocationName.str() << " == NULL) {\n";
			programFile << doubleIndent << varName << "Item->preConfigureLpsAllocation(";
			programFile << '"' << lpsName << '"' << paramSeparator;
			programFile << configName.str() << ")" << stmtSeparator;	
			programFile << indent << "} else {\n";
			programFile << doubleIndent << allocationName.str() << "->setPartitionConfig(";
			programFile << configName.str() << ")" << stmtSeparator;
			programFile << indent << "}\n";
		}	
	}	

	programFile << "}\n";
	
	headerFile.close();
	programFile.close();
}


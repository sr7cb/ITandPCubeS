#ifndef _H_ast_type
#define _H_ast_type

#include "ast.h"
#include "../../../common-libs/utils/list.h"
#include "../../../common-libs/utils/hashtable.h"

class VariableDef;

class Type : public Node {
  protected:
	const char *typeName;

  public:
    	static Type 	*intType, *floatType, *doubleType, *boolType, 
			*charType, *stringType, *dimensionType, 
			*rangeType, *indexType, *errorType, *voidType;

    	Type(yyltype loc) : Node(loc) {}
    	Type(const char *str);
	virtual ~Type() {}
	virtual const char *getName() { return typeName; }
    
    	const char *GetPrintNameForNode() { return "Type"; }
    	void PrintChildren(int indentLevel);

	//------------------------------------------------------------------ Helper functions for Semantic Analysis

        virtual Node *clone();
};

class NamedType : public Type {
  protected:
  	Identifier *id;
  public:
    	NamedType(Identifier *i);
    	const char *GetPrintNameForNode() { return "Named-Type"; }
   	void PrintChildren(int indentLevel);

	//------------------------------------------------------------------ Helper functions for Semantic Analysis

        Node *clone();
};

class ArrayType : public Type {
  protected:
    	Type *elemType;
	int dimensions;
  public:
    	ArrayType(yyltype loc, Type *elemType, int dimensions);
	virtual ~ArrayType() {}
    	const char *GetPrintNameForNode() { return "Dynamic-Array"; }
    	void PrintChildren(int indentLevel);

	//------------------------------------------------------------------ Helper functions for Semantic Analysis

        virtual Node *clone();
};

class StaticArrayType : public ArrayType {
  protected:
	List<int> *dimensionLengths;
  public:
	StaticArrayType(yyltype loc, Type *elemType, int dimensions) 
		: ArrayType(loc, elemType, dimensions) {}
    	const char *GetPrintNameForNode() { return "Static-Array"; }
    	void PrintChildren(int indentLevel);
	void setLengths(List<int> *dimensionLengths);

	//------------------------------------------------------------------ Helper functions for Semantic Analysis

        Node *clone();
};

class ListType : public Type {
  protected:
	Type *elemType;
  public:
	ListType(yyltype loc, Type *elemType);	
    	const char *GetPrintNameForNode() { return "List"; }
    	void PrintChildren(int indentLevel);

	//------------------------------------------------------------------ Helper functions for Semantic Analysis

        Node *clone();
};

class MapType : public Type {
  protected:
        Hashtable<VariableDef*> *elements;
  public:
        MapType(yyltype loc);
    	const char *GetPrintNameForNode() { return "Associated-List (Map)"; }
	const char *getName() { return "Map"; }
    	void PrintChildren(int indentLevel);
	bool hasElement(const char *elementName);
        Type *getElementType(const char *elementName);
        void setElement(VariableDef *newArg);
        VariableDef* getElement(const char *elementName);
        List<VariableDef*> *getElementList();

	//------------------------------------------------------------------ Helper functions for Semantic Analysis

        Node *clone();
};
 
#endif
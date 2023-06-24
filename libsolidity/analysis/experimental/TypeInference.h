/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0
#pragma once

#include <libsolidity/ast/ASTVisitor.h>
#include <libsolidity/ast/experimental/TypeSystem.h>

#include <liblangutil/ErrorReporter.h>

namespace solidity::frontend::experimental
{

class Analysis;

class TypeInference: public ASTConstVisitor
{
public:
	TypeInference(Analysis& _analysis);

	bool analyze(SourceUnit const& _sourceUnit);

	struct Annotation
	{
		/// Expressions, variable declarations, function declarations.
		std::optional<Type> type;
	};
	bool visit(Block const&) override { return true; }
	bool visit(VariableDeclarationStatement const&) override { return true; }
	bool visit(VariableDeclaration const& _variableDeclaration) override;

	bool visit(FunctionDefinition const& _functionDefinition) override;
	bool visit(ParameterList const& _parameterList) override;
	void endVisit(ParameterList const& _parameterList) override;
	bool visit(SourceUnit const&) override { return true; }
	bool visit(ContractDefinition const&) override { return true; }
	bool visit(InlineAssembly const& _inlineAssembly) override;
	bool visit(PragmaDirective const&) override { return false; }

	bool visit(ExpressionStatement const&) override { return true; }
	bool visit(Assignment const&) override;
	void endVisit(Assignment const&) override;
	bool visit(Identifier const&) override;
	bool visit(IdentifierPath const&) override;
	bool visit(FunctionCall const& _functionCall) override;
	void endVisit(FunctionCall const& _functionCall) override;
	bool visit(Return const&) override { return true; }
	void endVisit(Return const& _return) override;

	bool visit(MemberAccess const& _memberAccess) override;
	bool visit(ElementaryTypeNameExpression const& _expression) override;

	bool visit(TypeClassDefinition const& _typeClassDefinition) override;
	bool visit(TypeClassInstantiation const& _typeClassInstantiation) override;
	bool visit(TupleExpression const&) override { return true; }
	void endVisit(TupleExpression const& _tupleExpression) override;
	bool visit(TypeDefinition const& _typeDefinition) override;

	bool visitNode(ASTNode const& _node) override;

	bool visit(BinaryOperation const& _operation) override;
private:
	Analysis& m_analysis;
	langutil::ErrorReporter& m_errorReporter;
	TypeSystem& m_typeSystem;
	TypeEnvironment* m_env = nullptr;
	Type m_voidType;
	Type m_wordType;
	Type m_integerType;
	std::optional<Type> m_currentFunctionType;

	Annotation& annotation(ASTNode const& _node);

	void unify(Type _a, Type _b);
	enum class ExpressionContext
	{
		Term,
		Type,
		Sort
	};
	ExpressionContext m_expressionContext = ExpressionContext::Term;
};

}

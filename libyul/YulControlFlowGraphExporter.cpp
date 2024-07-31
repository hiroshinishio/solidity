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

#include <libyul/Utilities.h>
#include <libyul/YulControlFlowGraphExporter.h>

#include <libsolutil/Algorithms.h>
#include <libsolutil/Numeric.h>

#include <range/v3/view/transform.hpp>
#include <range/v3/view/map.hpp>

using namespace solidity;
using namespace solidity::langutil;
using namespace solidity::util;
using namespace solidity::yul;

YulControlFlowGraphExporter::YulControlFlowGraphExporter(SSACFG const& _ssacfg): m_ssacfg(_ssacfg)
{
}

// TODO: refactor and move to a general place, currently it depends on the SSACFG
std::string YulControlFlowGraphExporter::varToString(SSACFG::ValueId _var)
{
	if (_var.value == std::numeric_limits<size_t>::max())
		return std::string("INVALID");
	auto const& info = m_ssacfg.valueInfo(_var);
	return std::visit(
		util::GenericVisitor{
			[&](SSACFG::UnreachableValue const&) -> std::string {
				return "[unreachable]";
			},
			[&](SSACFG::PhiValue const&) {
				return "p" + std::to_string(_var.value);
			},
			[&](SSACFG::VariableValue const&) {
				return "v" + std::to_string(_var.value);
			},
			[&](SSACFG::LiteralValue const& _literal) {
				return toCompactHexWithPrefix(_literal.value);
			}
		},
		info
	);
}

Json YulControlFlowGraphExporter::run()
{
	Json yulObjectJson = Json::object();
	yulObjectJson["blocks"] = exportBlock(SSACFG::BlockId{0});

	Json functionsJson = Json::object();
	for (auto const& [function, functionInfo]: m_ssacfg.functionInfos)
		functionsJson[function->name.str()] = exportFunction(functionInfo);
	yulObjectJson["functions"] = functionsJson;

	return yulObjectJson;
}

Json YulControlFlowGraphExporter::exportFunction(SSACFG::FunctionInfo const& _functionInfo)
{
	Json functionJson = Json::object();
	functionJson["type"] = "Function";
	functionJson["entry"] = "Block" + std::to_string(_functionInfo.entry.value);
	functionJson["arguments"] = Json::array();
	for (auto const& [arg, valueId]: _functionInfo.arguments)
		functionJson["arguments"].emplace_back(arg.get().name.str());
	functionJson["returns"] = Json::array();
	for (auto const& ret: _functionInfo.returns)
		functionJson["returns"].emplace_back(ret.get().name.str());
	functionJson["blocks"] = exportBlock(_functionInfo.entry);
	return functionJson;
}

Json YulControlFlowGraphExporter::exportBlock(SSACFG::BlockId _entryId)
{
	Json blocksJson = Json::array();
	util::BreadthFirstSearch<SSACFG::BlockId> bfs{{{_entryId}}};
	bfs.run([&](SSACFG::BlockId _blockId, auto _addChild) {
		auto const& block = m_ssacfg.block(_blockId);
		// Convert current block to JSON
		blocksJson.emplace_back(toJson(_blockId));

		Json exitBlockJson = Json::object();
		exitBlockJson["id"] = "Block" + std::to_string(_blockId.value) + "Exit";
		exitBlockJson["instructions"] = Json::array();
		std::visit(util::GenericVisitor{
			[&](SSACFG::BasicBlock::MainExit const&) {
				exitBlockJson["exit"] = { "Block" + std::to_string(_blockId.value) };
				exitBlockJson["type"] = "MainExit";
			},
			[&](SSACFG::BasicBlock::Jump const& _jump)
			{
				exitBlockJson["exit"] = { "Block" + std::to_string(_jump.target.value) };
				exitBlockJson["type"] = "Jump";
				//TODO: handle backwards jump?
				_addChild(_jump.target);
			},
			[&](SSACFG::BasicBlock::ConditionalJump const& _conditionalJump)
			{
				exitBlockJson["exit"] = { "Block" + std::to_string(_conditionalJump.zero.value), "Block" + std::to_string(_conditionalJump.nonZero) };
				exitBlockJson["cond"] = varToString(_conditionalJump.condition);
				exitBlockJson["type"] = "ConditionalJump";

				_addChild(_conditionalJump.zero);
				_addChild(_conditionalJump.nonZero);
			},
			[&](SSACFG::BasicBlock::FunctionReturn const& _return) {
				exitBlockJson["instructions"] = toJson(_return.returnValues);
				exitBlockJson["exit"] = { "Block" + std::to_string(_blockId.value) };
				exitBlockJson["type"] = "FunctionReturn";
			},
			[&](SSACFG::BasicBlock::Terminated const&) {
				exitBlockJson["exit"] = { "Block" + std::to_string(_blockId.value) };
				exitBlockJson["type"] = "Terminated";
			},
			[&](auto& ){ /*FIXME: remove jump table and fix switch */}
		}, block.exit);
		blocksJson.emplace_back(exitBlockJson);
	});

	return blocksJson;
}

Json YulControlFlowGraphExporter::toJson(SSACFG::BlockId _blockId)
{
	Json blockJson = Json::object();
	auto const& block = m_ssacfg.block(_blockId);

	blockJson["id"] = "Block" + std::to_string(_blockId.value);
	blockJson["instructions"] = Json::array();
	for (auto const& operation: block.operations)
		blockJson["instructions"].push_back(toJson(blockJson, operation));
	blockJson["exit"] = "Block" + std::to_string(_blockId.value) + "Exit";

	return blockJson;
}

Json YulControlFlowGraphExporter::toJson(Json& _ret, SSACFG::Operation const& _operation)
{
	Json opJson = Json::object();
	// TODO: block.phis
	std::vector<SSACFG::ValueId> inputs = _operation.inputs;
	std::visit(util::GenericVisitor{
		[&](SSACFG::Call const& _call) {
			solAssert(m_ssacfg.functionInfos.count(&_call.function.get()), "FunctionCall must have a corresponding FunctionInfo");
			auto functionInfo = m_ssacfg.functionInfos.at(&_call.function.get());
			if (functionInfo.canContinue)
			{
				solAssert(!inputs.empty(), "FunctionCall must have a return label as first input");
				// TODO: check return label and pop it from the input
				//if (auto* returnLabelSlot = std::get_if<FunctionCallReturnLabelSlot>(&input.front()))
				//{
				//	solAssert(&returnLabelSlot->call.get() == &_call.functionCall.get(), "FunctionCallReturnLabelSlot must refer to the same function as the FunctionCall");
				//	// remove the return label from the input
				//	input.erase(input.begin());
				//}
			}
			_ret["type"] = "FunctionCall";
			opJson["op"] = _call.function.get().name.str();
		},
		[&](SSACFG::BuiltinCall const& _call) {
			_ret["type"] = "BuiltinCall";
			Json builtinArgsJson = Json::array();
			auto const& builtin = _call.builtin.get();
			if (!builtin.literalArguments.empty())
			{
				auto const& functionCallArgs = _call.call.get().arguments;
				for (size_t i = 0; i < builtin.literalArguments.size(); ++i)
				{
					std::optional<LiteralKind> const& argument = builtin.literalArguments[i];
					if (argument.has_value() && i < functionCallArgs.size())
					{
						// The function call argument at index i must be a literal if builtin.literalArguments[i] is not nullopt
						yulAssert(std::holds_alternative<Literal>(functionCallArgs[i]));
						builtinArgsJson.push_back(formatLiteral(std::get<Literal>(functionCallArgs[i])));
					}
				}
			}

			if (!builtinArgsJson.empty())
				opJson["builtinArgs"] = builtinArgsJson;

			opJson["op"] = _call.builtin.get().name.str();
		},
		//[&](SSACFG::Assignment const& _assignment) {
		//	_ret["type"] = "Assignment";
		//	Json::array_t assignmentJson;
		//	for (auto const& variable: _assignment.variables)
		//		assignmentJson.push_back(varToString(variable));
		//	opJson["assignment"] = assignmentJson;
		//}
	}, _operation.kind);

	opJson["in"] = toJson(inputs);
	opJson["out"] = toJson(_operation.outputs);

	return opJson;
}

Json YulControlFlowGraphExporter::toJson(std::vector<SSACFG::ValueId> const& _values)
{
	Json ret = Json::array();
	for (auto const& value: _values)
		ret.push_back(varToString(value));
	return ret;
}

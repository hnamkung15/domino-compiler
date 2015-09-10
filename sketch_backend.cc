#include "sketch_backend.h"

#include "third_party/assert_exception.h"

#include "clang_utility_functions.h"
#include "set_idioms.h"

using namespace clang;

std::string sketch_backend_transform(const TranslationUnitDecl * tu_decl) {
  std::string ret;
  for (const auto * child_decl : dyn_cast<DeclContext>(tu_decl)->decls()) {
    if (isa<FunctionDecl>(child_decl) and (is_packet_func(dyn_cast<FunctionDecl>(child_decl)))) {
      ret += (create_sketch_spec(dyn_cast<FunctionDecl>(child_decl)->getBody()));
    }
  }
  return ret;
}

std::string create_sketch_spec(const Stmt * function_body) {
  // Store all MemberExpr from the function_body.
  // This gives all packet fields seen in function_body
  std::set<PktField> all_pkt_fields = gen_var_list(function_body,
                                                   {{VariableType::PACKET, true},
                                                    {VariableType::STATE_SCALAR, false},
                                                    {VariableType::STATE_ARRAY, false}});

  // Store all fields that are written into
  std::set<PktField> defined_fields;
  for (const auto * stmt : function_body->children()) {
    assert_exception(isa<BinaryOperator>(stmt));
    const auto * bin_op = dyn_cast<BinaryOperator>(stmt);
    assert_exception(bin_op->isAssignmentOp());
    if (isa<MemberExpr>(bin_op->getLHS())) {
      defined_fields.emplace(clang_stmt_printer(dyn_cast<MemberExpr>(bin_op->getLHS())));
    }
  }

  // Subtract defined_fields from all_pkt_fields to get incoming fields
  // that are set by someone else before getting into this function body
  std::set<PktField> incoming_fields = all_pkt_fields - defined_fields;

  // Create unique names for all fields in incoming_fields.
  std::map<PktField, ScalarVarName> rename_map;
  uint8_t count = 0;
  std::set<std::string> pkt_var_args;
  for (const auto & field : incoming_fields) {
    rename_map[field] = "pkt_" + std::to_string(++count);
    pkt_var_args.emplace(rename_map.at(field));
  }

  // Conjoin object name with field to create scalar variables
  // for all packet fields that are defined in the body
  // i.e. change p.x to p_x
  for (const auto & field : defined_fields) {
    std::string conjoined_field = field;
    std::replace(conjoined_field.begin(), conjoined_field.end(), '.', '_');
    rename_map[field] = conjoined_field;
  }

  // Collect all state variables seen in the function body
  std::set<std::string> state_refs = collect_state_vars(function_body);

  // Create unique names for all state variables
  count = 0;
  std::set<std::string> state_var_args;
  for (const auto & state_ref : state_refs) {
    rename_map[state_ref] = "state_" + std::to_string(++count);
    state_var_args.emplace(rename_map.at(state_ref));
  }

  // Add function signature
  std::string sketch_spec_signature = "void spec(";
  bool empty = true;
  for (const auto & arg : state_var_args + pkt_var_args) {
    empty = false;
    sketch_spec_signature += "int " + arg + ",";
  }
  if (empty) sketch_spec_signature += ")";
  else sketch_spec_signature.back() = ')';

  // Now, go through the function_body, renaming everything in the process.
  std::string sketch_spec_body = "{\n";
  for (const auto * child : function_body->children()) {
    assert_exception(isa<BinaryOperator>(child));
    const auto * bin_op = dyn_cast<BinaryOperator>(child);
    assert_exception(bin_op->isAssignmentOp());
    sketch_spec_body += replace_vars(bin_op->getLHS(), rename_map,
                                     {{VariableType::STATE_SCALAR, true}, {VariableType::STATE_ARRAY, true}, {VariableType::PACKET, true}})
                        + "=" +
                        replace_vars(bin_op->getRHS(), rename_map,
                                    {{VariableType::STATE_SCALAR, true}, {VariableType::STATE_ARRAY, true}, {VariableType::PACKET, true}})
                        + ";\n";
  }
  sketch_spec_body += "}";
  return sketch_spec_signature + "\n" + sketch_spec_body;
}

std::set<std::string> collect_state_vars(const Stmt * stmt) {
  // Recursively scan stmt to generate a set of strings representing
  // either packet fields or state variables used within stmt
  assert_exception(stmt);
  std::set<std::string> ret;
  if (isa<CompoundStmt>(stmt)) {
    for (const auto & child : stmt->children()) {
      ret = ret + collect_state_vars(child);
    }
    return ret;
  } else if (isa<BinaryOperator>(stmt)) {
    const auto * bin_op = dyn_cast<BinaryOperator>(stmt);
    return collect_state_vars(bin_op->getLHS()) + collect_state_vars(bin_op->getRHS());
  } else if (isa<ConditionalOperator>(stmt)) {
    const auto * cond_op = dyn_cast<ConditionalOperator>(stmt);
    return collect_state_vars(cond_op->getCond()) + collect_state_vars(cond_op->getTrueExpr()) + collect_state_vars(cond_op->getFalseExpr());
  } else if (isa<MemberExpr>(stmt)) {
    return std::set<std::string>();
  } else if (isa<DeclRefExpr>(stmt)) {
    return std::set<std::string>{clang_stmt_printer(stmt)};
  } else if (isa<ArraySubscriptExpr>(stmt)) {
    // We return array name here, not array name subscript index.
    return std::set<std::string>{clang_stmt_printer(stmt)};
  } else if (isa<IntegerLiteral>(stmt) or isa<NullStmt>(stmt)) {
    return std::set<std::string>();
  } else if (isa<ParenExpr>(stmt)) {
    return collect_state_vars(dyn_cast<ParenExpr>(stmt)->getSubExpr());
  } else if (isa<UnaryOperator>(stmt)) {
    const auto * un_op = dyn_cast<UnaryOperator>(stmt);
    assert_exception(un_op->isArithmeticOp());
    const auto opcode_str = std::string(UnaryOperator::getOpcodeStr(un_op->getOpcode()));
    assert_exception(opcode_str == "!");
    return collect_state_vars(un_op->getSubExpr());
  } else if (isa<ImplicitCastExpr>(stmt)) {
    return collect_state_vars(dyn_cast<ImplicitCastExpr>(stmt)->getSubExpr());
  } else if (isa<CallExpr>(stmt)) {
    const auto * call_expr = dyn_cast<CallExpr>(stmt);
    std::set<std::string> ret;
    for (const auto * child : call_expr->arguments()) {
      const auto child_uses = collect_state_vars(child);
      ret = ret + child_uses;
    }
    return ret;
  } else {
    throw std::logic_error("collect_state_vars cannot handle stmt of type " + std::string(stmt->getStmtClassName()));
  }
}

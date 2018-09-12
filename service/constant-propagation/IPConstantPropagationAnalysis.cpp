/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "IPConstantPropagationAnalysis.h"

namespace constant_propagation {

namespace interprocedural {

/*
 * Return an environment populated with parameter values.
 */
ConstantEnvironment env_with_params(const IRCode* code,
                                    const ArgumentDomain& args) {
  size_t idx{0};
  ConstantEnvironment env;
  for (auto& mie : InstructionIterable(code->get_param_instructions())) {
    env.set(mie.insn->dest(), args.get(idx++));
  }
  return env;
}

void FixpointIterator::analyze_node(DexMethod* const& method,
                                    Domain* current_state) const {
  // The entry node has no associated method.
  if (method == nullptr) {
    return;
  }
  auto code = method->get_code();
  if (code == nullptr) {
    return;
  }
  auto& cfg = code->cfg();
  auto intra_cp = get_intraprocedural_analysis(method);

  for (auto* block : cfg.blocks()) {
    auto state = intra_cp->get_entry_state_at(block);
    for (auto& mie : InstructionIterable(block)) {
      auto* insn = mie.insn;
      auto op = insn->opcode();
      if (op == OPCODE_INVOKE_DIRECT || op == OPCODE_INVOKE_STATIC) {
        ArgumentDomain out_args;
        for (size_t i = 0; i < insn->srcs_size(); ++i) {
          out_args.set(i, state.get(insn->src(i)));
        }
        current_state->set(insn, out_args);
      }
      intra_cp->analyze_instruction(insn, &state);
    }
  }
}

Domain FixpointIterator::analyze_edge(
    const std::shared_ptr<call_graph::Edge>& edge,
    const Domain& exit_state_at_source) const {
  Domain entry_state_at_dest;
  auto it = edge->invoke_iterator();
  if (it == IRList::iterator()) {
    entry_state_at_dest.set(CURRENT_PARTITION_LABEL, ArgumentDomain::top());
  } else {
    auto insn = it->insn;
    entry_state_at_dest.set(CURRENT_PARTITION_LABEL,
                            exit_state_at_source.get(insn));
  }
  return entry_state_at_dest;
}

std::unique_ptr<intraprocedural::FixpointIterator>
FixpointIterator::get_intraprocedural_analysis(const DexMethod* method) const {
  auto args = this->get_entry_state_at(const_cast<DexMethod*>(method));
  return m_proc_analysis_factory(method,
                                 this->get_whole_program_state(),
                                 args.get(CURRENT_PARTITION_LABEL));
}

} // namespace interprocedural

void set_encoded_values(const DexClass* cls, ConstantEnvironment* env) {
  for (auto* sfield : cls->get_sfields()) {
    auto value = sfield->get_static_value();
    if (value == nullptr) {
      env->set(sfield, SignedConstantDomain(0));
    } else if (is_primitive(sfield->get_type())) {
      env->set(sfield, SignedConstantDomain(value->value()));
    } else {
      env->set(sfield, SignedConstantDomain::top());
    }
  }
}

} // namespace constant_propagation

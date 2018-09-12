/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#pragma once

#include <boost/optional/optional.hpp>
#include <type_traits>
#include <utility>

#include "FixpointIterators.h"
#include "IRCode.h"

/**
 * A Control Flow Graph is a directed graph of Basic Blocks.
 *
 * Each `Block` has some number of successors and predecessors. `Block`s are
 * connected to their predecessors and successors by `Edge`s that specify
 * the type of connection.
 *
 * Right now there are two types of CFGs. Editable and non-editable:
 * A non editable CFG's blocks have begin and end pointers into the big linear
 * IRList inside IRCode.
 * An editable CFG's blocks each own a small IRList (with MethodItemEntries
 * taken from IRCode)
 *
 * EDITABLE MODE:
 * It is easier to maintain the data structure when there is no unnecessary
 * information duplication. Therefore, MFLOW_TARGETs and OPCODE_GOTOs are
 * deleted and their information is moved to the edges of the CFG.
 *
 * TODO: Add useful CFG editing methods
 * TODO: phase out edits to the IRCode and move them all to the editable CFG
 * TODO: remove non-editable CFG option
 *
 * TODO?: remove unreachable blocks in simplify?
 * TODO?: remove items instead of replacing with MFLOW_FALLTHROUGH?
 * TODO?: make MethodItemEntry's fields private?
 */

namespace cfg {

enum EdgeType { EDGE_GOTO, EDGE_BRANCH, EDGE_THROW, EDGE_TYPE_SIZE };

class Block;
class ControlFlowGraph;

struct ThrowInfo {
  DexType* catch_type;
  // Index from the linked list of `CatchEntry`s in the IRCode. An index of 0
  // denotes the "primary" catch. At runtime, the primary catch is the first
  // block to be checked for equality with the runtime exception type, then onto
  // index 1, etc.
  uint32_t index;

  ThrowInfo(DexType* catch_type, uint32_t index)
      : catch_type(catch_type), index(index) {}

  bool operator==(const ThrowInfo& other) {
    return catch_type == other.catch_type && index == other.index;
  }
};

class Edge final {
 public:
  using CaseKey = int32_t;

 private:
  Block* m_src;
  Block* m_target;
  EdgeType m_type;

  // If this branch is a non-default case of a switch statement, this is the
  // index of the corresponding case block.
  boost::optional<CaseKey> m_case_key;

  std::unique_ptr<ThrowInfo> m_throw_info{nullptr};

  friend class ControlFlowGraph;
  friend class Block;

 public:
  Edge(Block* src, Block* target, EdgeType type)
      : m_src(src), m_target(target), m_type(type) {
    always_assert_log(m_type != EDGE_THROW,
                      "Need a catch type and index to create a THROW edge");
  }
  Edge(Block* src, Block* target, CaseKey case_key)
      : m_src(src),
        m_target(target),
        m_type(EDGE_BRANCH),
        m_case_key(case_key) {}
  Edge(Block* src, Block* target, DexType* catch_type, uint32_t index)
      : m_src(src),
        m_target(target),
        m_type(EDGE_THROW),
        m_throw_info(std::make_unique<ThrowInfo>(catch_type, index)) {}

  bool operator==(const Edge& that) const {
    return m_src == that.m_src && m_target == that.m_target &&
           equals_ignore_source_and_target(that);
  }

  bool equals_ignore_source(const Edge& that) const {
    return m_target == that.m_target && equals_ignore_source_and_target(that);
  }

  bool equals_ignore_target(const Edge& that) const {
    return m_src == that.m_src && equals_ignore_source_and_target(that);
  }

  bool equals_ignore_source_and_target(const Edge& that) const {
    return m_type == that.m_type &&
           ((m_throw_info == nullptr && that.m_throw_info == nullptr) ||
            *m_throw_info == *that.m_throw_info);
  }

  Block* src() const { return m_src; }
  Block* target() const { return m_target; }
  EdgeType type() const { return m_type; }
  boost::optional<CaseKey> case_key() const { return m_case_key; }
};

std::ostream& operator<<(std::ostream& os, const Edge& e);

using BlockId = size_t;

template <bool is_const>
class InstructionIteratorImpl;
using InstructionIterator = InstructionIteratorImpl</* is_const */ false>;
using ConstInstructionIterator = InstructionIteratorImpl</* is_const */ true>;

template <bool is_const>
class InstructionIterableImpl;
using InstructionIterable = InstructionIterableImpl</* is_const */ false>;
using ConstInstructionIterable = InstructionIterableImpl</* is_const */ true>;

// A piece of "straight-line" code. Targets are only at the beginning of a block
// and branches (throws, gotos, switches, etc) are only at the end of a block.
class Block {
 public:
  explicit Block(const ControlFlowGraph* parent, BlockId id)
      : m_id(id), m_parent(parent) {}

  ~Block() {
    m_entries.clear_and_dispose();
  }

  BlockId id() const { return m_id; }
  const std::vector<Edge*>& preds() const {
    return m_preds;
  }
  const std::vector<Edge*>& succs() const {
    return m_succs;
  }

  bool operator<(const Block& other) const {
    return this->id() < other.id();
  }

  // return true if `b` is a predecessor of this.
  // optionally supply a specific type of predecessor. The default,
  // EDGE_TYPE_SIZE, means any type
  bool has_pred(Block* b, EdgeType t = EDGE_TYPE_SIZE) const;

  // return true if `b` is a successor of this.
  // optionally supply a specific type of successor. The default,
  // EDGE_TYPE_SIZE, means any type
  bool has_succ(Block* b, EdgeType t = EDGE_TYPE_SIZE) const;

  IRList::iterator begin();
  IRList::iterator end();
  IRList::const_iterator begin() const;
  IRList::const_iterator end() const;
  IRList::reverse_iterator rbegin() { return IRList::reverse_iterator(end()); }
  IRList::reverse_iterator rend() { return IRList::reverse_iterator(begin()); }

  bool is_catch() const;

  bool same_try(Block* other) const;

  void remove_opcode(const IRList::iterator& it);

  opcode::Branchingness branchingness() {
    always_assert_log(
        !empty(), "block %d is empty\n%s\n", id(), SHOW(*m_parent));
    const auto& last = rbegin();
    return last->branchingness();
  }

  bool empty() const { return m_entries.empty(); }

  IRList::iterator get_last_insn();
  IRList::iterator get_first_insn();

  // including move-result-pseudo
  bool starts_with_move_result();

  // Remove the first target in this block that corresponds to `branch`.
  // Returns a not-none CaseKey for multi targets, boost::none otherwise.
  boost::optional<Edge::CaseKey> remove_first_matching_target(
      MethodItemEntry* branch);

 private:
  friend class ControlFlowGraph;
  friend class InstructionIteratorImpl<false>;
  friend class InstructionIteratorImpl<true>;

  // return an iterator to the conditional branch (including switch) in this
  // block. If there is no such instruction, return end()
  IRList::iterator get_conditional_branch();

  BlockId m_id;

  // MethodItemEntries get moved from IRCode into here (if m_editable)
  // otherwise, this is empty.
  IRList m_entries;

  // TODO delete these
  // These refer into the IRCode IRList
  // These are only used in non-editable mode.
  IRList::iterator m_begin;
  IRList::iterator m_end;

  std::vector<Edge*> m_preds;
  std::vector<Edge*> m_succs;

  // the graph that this block belongs to
  const ControlFlowGraph* m_parent = nullptr;
};

struct DominatorInfo {
  Block* dom;
  size_t postorder;
};

class ControlFlowGraph {

 public:
  ControlFlowGraph() = default;
  ControlFlowGraph(const ControlFlowGraph&) = delete;

  /*
   * if editable is false, changes to the CFG aren't reflected in the output dex
   * instructions.
   */
  ControlFlowGraph(IRList* ir, bool editable = false);
  ~ControlFlowGraph();

  /*
   * convert from the graph representation to a list of MethodItemEntries
   */
  IRList* linearize();

  std::vector<Block*> blocks() const;

  Block* create_block();
  const Block* entry_block() const { return m_entry_block; }
  const Block* exit_block() const { return m_exit_block; }
  Block* entry_block() { return m_entry_block; }
  Block* exit_block() { return m_exit_block; }
  void set_entry_block(Block* b) { m_entry_block = b; }
  void set_exit_block(Block* b) { m_exit_block = b; }
  /*
   * Determine where the exit block is. If there is more than one, create a
   * "ghost" block that is the successor to all of them.
   */
  void calculate_exit_block();

  // args are arguments to an Edge constructor
  template <class... Args>
  void add_edge(Args&&... args);

  // Remove this edge from the graph but do not release its memory.
  // This can be used to temporarily remove an edge while moving it elsewhere.
  void remove_edge(Edge* edge);

  using EdgePredicate = std::function<bool(const Edge* e)>;

  void remove_edge_if(Block* source,
                      Block* target,
                      const EdgePredicate& predicate);

  void remove_succ_edge_if(Block* block, const EdgePredicate& predicate);

  void remove_pred_edge_if(Block* block, const EdgePredicate& predicate);

  void remove_succ_edges(Block* b);
  void remove_pred_edges(Block* b);

  // Make `e` point to a new target block.
  // The source block is unchanged.
  void set_edge_target(Edge* e, Block* new_target);

  // Make `e` come from a new source block
  // The target block is unchanged.
  void set_edge_source(Edge* e, Block* source_target);

  // return the first edge for which predicate returns true
  // or nullptr if no such edge exists
  Edge* get_pred_edge_if(
      const Block* block, const EdgePredicate& predicate) const;
  Edge* get_succ_edge_if(
      const Block* block, const EdgePredicate& predicate) const;

  // return all edges for which predicate returns true
  std::vector<Edge*> get_pred_edges_if(
      const Block* block, const EdgePredicate& predicate) const;
  std::vector<Edge*> get_succ_edges_if(
      const Block* block, const EdgePredicate& predicate) const;

  bool blocks_are_in_same_try(const Block* b1, const Block* b2) const;

  // merge succ into pred
  // Assumption: pred is the only predecessor of succ (and vice versa)
  void merge_blocks(Block* pred, Block* succ);

  // remove the IRInstruction that `it` points to.
  //
  // If `it` points to a branch instruction, remove the corresponding outgoing
  // edges.
  void remove_opcode(const InstructionIterator& it);

  // delete old_block and reroute its predecessors to new_block
  void replace_block(Block* old_block, Block* new_block);

  // Remove this block from the graph and release associated memory.
  // Remove all incoming and outgoing edges.
  void remove_block(Block* block);

  /*
   * Print the graph in the DOT graph description language.
   */
  std::ostream& write_dot_format(std::ostream&) const;

  // Find a common dominator block that is closest to both block.
  Block* idom_intersect(
      const std::unordered_map<Block*, DominatorInfo>& postorder_dominator,
      Block* block1,
      Block* block2) const;

  // Finding immediate dominator for each blocks in ControlFlowGraph.
  std::unordered_map<Block*, DominatorInfo> immediate_dominators() const;

  // Do writes to this CFG propagate back to IR and Dex code?
  bool editable() const { return m_editable; }

  size_t num_blocks() const { return m_blocks.size(); }

  // remove blocks with no predecessors
  void remove_unreachable_blocks();

  // transform the CFG to an equivalent but more canonical state
  // Assumes m_editable is true
  void simplify();

  // SIGABORT if the internal state of the CFG is invalid
  void sanity_check();

  // SIGABORT if there are dangling parent pointers to deleted DexPositions
  void no_dangling_dex_positions();

 private:
  using BranchToTargets =
      std::unordered_map<MethodItemEntry*, std::vector<Block*>>;
  using TryEnds = std::vector<std::pair<TryEntry*, Block*>>;
  using TryCatches = std::unordered_map<CatchEntry*, Block*>;
  using Boundaries =
      std::unordered_map<Block*, std::pair<IRList::iterator, IRList::iterator>>;
  using Blocks = std::map<BlockId, Block*>;
  using Edges = std::vector<Edge*>;
  friend class InstructionIteratorImpl<false>;
  friend class InstructionIteratorImpl<true>;

  // Find block boundaries in IRCode and create the blocks
  // For use by the constructor. You probably don't want to call this from
  // elsewhere
  void find_block_boundaries(IRList* ir,
                             BranchToTargets& branch_to_targets,
                             TryEnds& try_ends,
                             TryCatches& try_catches,
                             Boundaries& boundaries);

  // Add edges between blocks created by `find_block_boundaries`
  // For use by the constructor. You probably don't want to call this from
  // elsewhere
  void connect_blocks(BranchToTargets& branch_to_targets);

  // Add edges from try blocks to their catch handlers.
  // For use by the constructor. You probably don't want to call this from
  // elsewhere
  void add_catch_edges(TryEnds& try_ends, TryCatches& try_catches);

  // For use by the constructor. You probably don't want to call this from
  // elsewhere
  void remove_unreachable_succ_edges();

  // Move MethodItemEntries from ir into their blocks
  // For use by the constructor. You probably don't want to call this from
  // elsewhere
  void fill_blocks(IRList* ir, const Boundaries& boundaries);

  // remove all MFLOW_TRY and MFLOW_CATCH markers because that information is
  // moved into the edges.
  // Assumes m_editable is true
  void remove_try_catch_markers();

  // choose an order of blocks for output
  std::vector<Block*> order();

  // Materialize target instructions and gotos corresponding to control-flow
  // edges. Used while turning back into a linear representation.
  void insert_branches_and_targets(const std::vector<Block*>& ordering);

  // Create MFLOW_CATCH entries for each EDGE_THROW. returns the primary
  // MFLOW_CATCH (first element in linked list of CatchEntries) on a block that
  // can throw. returns nullptr on a block without outgoing throw edges. This
  // function is used while turning back into a linear representation.
  //
  // Example: For a block with outgoing edges
  //
  // Edge(block, catch_block_1, FooException, 1)
  // Edge(block, catch_block_2, BarException, 2)
  // Edge(block, catch_block_3, BazException, 3)
  //
  // This generates CatchEntries (linked list)
  //
  // CatchEntry(FooException) ->
  //   CatchEntry(BarException) ->
  //     CatchEntry(BazException)
  MethodItemEntry* create_catch(
      Block* block,
      std::unordered_map<MethodItemEntry*, Block*>* catch_to_containing_block);

  // Materialize TRY_STARTs, TRY_ENDs, and MFLOW_CATCHes
  // Used while turning back into a linear representation.
  void insert_try_catch_markers(const std::vector<Block*>& ordering);

  // Follow the catch entry linked list starting at `first_mie` and
  // make sure the throw edges (pointed to by `it`) agree with the linked list.
  // Used while turning back into a linear representation.
  bool catch_entries_equivalent_to_throw_edges(
      MethodItemEntry* first_mie,
      std::vector<Edge*>::iterator it,
      std::vector<Edge*>::iterator end,
      const std::unordered_map<MethodItemEntry*, Block*>&
          catch_to_containing_block);

  void remove_all_edges(Block* pred, Block* succ);

  // Move edge between new_source and new_target.
  // If either new_source or new_target is null, don't change that field of the
  // edge
  void move_edge(Edge* edge, Block* new_source, Block* new_target);

  // The memory of all blocks and edges in this graph are owned here
  Blocks m_blocks;
  Edges m_edges;

  Block* m_entry_block{nullptr};
  Block* m_exit_block{nullptr};
  bool m_editable;
};

// A static-method-only API for use with the monotonic fixpoint iterator.
class GraphInterface {

 public:
  using Graph = ControlFlowGraph;
  using NodeId = Block*;
  using EdgeId = Edge*;
  static NodeId entry(const Graph& graph) {
    return const_cast<NodeId>(graph.entry_block());
  }
  static NodeId exit(const Graph& graph) {
    return const_cast<NodeId>(graph.exit_block());
  }
  static std::vector<EdgeId> predecessors(const Graph&, const NodeId& b) {
    return b->preds();
  }
  static std::vector<EdgeId> successors(const Graph&, const NodeId& b) {
    return b->succs();
  }
  static NodeId source(const Graph&, const EdgeId& e) { return e->src(); }
  static NodeId target(const Graph&, const EdgeId& e) { return e->target(); }
};

template <bool is_const>
class InstructionIteratorImpl {
  using Cfg = typename std::
      conditional<is_const, const ControlFlowGraph, ControlFlowGraph>::type;
  using Mie = typename std::
      conditional<is_const, const MethodItemEntry, MethodItemEntry>::type;
  using Iterator = typename std::
      conditional<is_const, IRList::const_iterator, IRList::iterator>::type;

  Cfg& m_cfg;
  ControlFlowGraph::Blocks::const_iterator m_block;

  // Depends on C++14 Null Forward Iterators
  // Assuming the default constructed InstructionIterator compares equal
  // to other default constructed InstructionIterator
  //
  // boost.org/doc/libs/1_58_0/doc/html/container/Cpp11_conformance.html
  ir_list::InstructionIteratorImpl<is_const> m_it;

  // go to beginning of next block, skipping empty blocks
  void to_next_block() {
    while (
        m_block != m_cfg.m_blocks.end() &&
        m_it ==
            ir_list::InstructionIterableImpl<is_const>(m_block->second).end()) {
      ++m_block;
      if (m_block != m_cfg.m_blocks.end()) {
        m_it =
            ir_list::InstructionIterableImpl<is_const>(m_block->second).begin();
      } else {
        m_it = ir_list::InstructionIteratorImpl<is_const>();
      }
    }
  }

 public:
  using reference = Mie&;
  using difference_type = long;
  using value_type = Mie&;
  using pointer = Mie*;
  using iterator_category = std::forward_iterator_tag;

  InstructionIteratorImpl() = delete;
  explicit InstructionIteratorImpl(Cfg& cfg, bool is_begin)
      : m_cfg(cfg) {
    always_assert(m_cfg.editable());
    if (is_begin) {
      m_block = m_cfg.m_blocks.begin();
      m_it =
          ir_list::InstructionIterableImpl<is_const>(m_block->second).begin();
    } else {
      m_block = m_cfg.m_blocks.end();
    }
  }

  InstructionIteratorImpl<is_const>& operator++() {
    assert_not_end();
    ++m_it;
    to_next_block();
    return *this;
  }

  InstructionIteratorImpl<is_const> operator++(int) {
    auto result = *this;
    ++(*this);
    return result;
  }

  reference operator*() const {
    assert_not_end();
    return *m_it;
  }

  pointer operator->() const { return &(this->operator*()); }

  bool operator==(const InstructionIteratorImpl& other) const {
    return this->m_block == other.m_block && this->m_it == other.m_it;
  }

  bool operator!=(const InstructionIteratorImpl& other) const {
    return !(*this == other);
  }

  void assert_not_end() const {
    always_assert(m_block != m_cfg.m_blocks.end());
    always_assert(
        m_it !=
        ir_list::InstructionIterableImpl<is_const>(m_block->second).end());
  }

  Iterator unwrap() const {
    return m_it.unwrap();
  }

  Block* block() const {
    assert_not_end();
    return m_block->second;
  }
};

// Iterate through all IRInstructions in the CFG.
// Instructions in the same block are processed in order.
// Blocks are iterated in an undefined order
template <bool is_const>
class InstructionIterableImpl {
  using Cfg = typename std::conditional<is_const, const ControlFlowGraph, ControlFlowGraph>::type;
  using Iterator = typename std::conditional<is_const, IRList::const_iterator, IRList::iterator>::type;
  Cfg& m_cfg;

 public:
  InstructionIterableImpl() = delete;
  explicit InstructionIterableImpl(Cfg& cfg) : m_cfg(cfg) {}

  InstructionIteratorImpl<is_const> begin() {
    return InstructionIteratorImpl<is_const>(m_cfg, true);
  }

  InstructionIteratorImpl<is_const> end() {
    return InstructionIteratorImpl<is_const>(m_cfg, false);
  }

  bool empty() { return begin() == end(); }
};

std::vector<Block*> find_exit_blocks(const ControlFlowGraph&);

/*
 * Build a postorder sorted vector of blocks from the given CFG. Uses a
 * standard depth-first search with a side table of already-visited nodes.
 */
std::vector<Block*> postorder_sort(const std::vector<Block*>& cfg);

} // namespace cfg

inline cfg::InstructionIterable InstructionIterable(cfg::ControlFlowGraph& cfg) {
  return cfg::InstructionIterable(cfg);
}

inline cfg::ConstInstructionIterable InstructionIterable(
    const cfg::ControlFlowGraph& cfg) {
  return cfg::ConstInstructionIterable(cfg);
}

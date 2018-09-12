/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "InterDex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <unordered_set>

#include "file-utils.h"
#include "ConfigFiles.h"
#include "Creators.h"
#include "Debug.h"
#include "DexClass.h"
#include "DexLoader.h"
#include "DexOutput.h"
#include "DexUtil.h"
#include "IRCode.h"
#include "IRInstruction.h"
#include "ReachableClasses.h"
#include "StringUtil.h"
#include "Walkers.h"

namespace {

typedef std::unordered_set<DexMethodRef*> mrefs_t;
typedef std::unordered_set<DexFieldRef*> frefs_t;

size_t global_dmeth_cnt;
size_t global_smeth_cnt;
size_t global_vmeth_cnt;
size_t global_methref_cnt;
size_t global_fieldref_cnt;
size_t global_cls_cnt;
size_t cls_skipped_in_primary = 0;
size_t cls_skipped_in_secondary = 0;
size_t cold_start_set_dex_count = 1000;

bool emit_canaries = false;
int64_t linear_alloc_limit;
std::unordered_set<DexClass*> mixed_mode_classes;

void gather_refs(InterDexPass* pass,
                 const DexClass* cls,
                 mrefs_t* mrefs,
                 frefs_t* frefs) {
  std::vector<DexMethodRef*> method_refs;
  std::vector<DexFieldRef*> field_refs;
  cls->gather_methods(method_refs);
  cls->gather_fields(field_refs);
  for (const auto& plugin : pass->m_plugins) {
    plugin->gather_mrefs(cls, method_refs, field_refs);
  }
  mrefs->insert(method_refs.begin(), method_refs.end());
  frefs->insert(field_refs.begin(), field_refs.end());
}

/*
 * Removes the elements of b from a. Runs in O(size(a)), so it works best if
 * size(a) << size(b).
 */
template <typename T>
std::unordered_set<T> set_difference(const std::unordered_set<T>& a,
                                     const std::unordered_set<T>& b) {
  std::unordered_set<T> result;
  for (auto& v : a) {
    if (!b.count(v)) {
      result.emplace(v);
    }
  }
  return result;
}

constexpr int kMaxMethodRefs = ((64 * 1024) - 1);
constexpr int kMaxFieldRefs = 64 * 1024 - 1;
constexpr char kCanaryPrefix[] = "Lsecondary/dex";
constexpr char kCanaryClassFormat[] = "Lsecondary/dex%02d/Canary;";
constexpr size_t kCanaryClassBufsize = sizeof(kCanaryClassFormat);
constexpr int kMaxDexNum = 99;

struct dex_emit_tracker {
  unsigned la_size{0};
  mrefs_t mrefs;
  frefs_t frefs;
  std::vector<DexClass*> outs;
  std::unordered_set<DexClass*> emitted;
  std::unordered_map<std::string, DexClass*> clookup;

  void start_new_dex() {
    la_size = 0;
    mrefs.clear();
    frefs.clear();
    outs.clear();
  }
};

void update_dex_stats(size_t cls_cnt, size_t methrefs_cnt, size_t frefs_cnt) {
  global_cls_cnt += cls_cnt;
  global_methref_cnt += methrefs_cnt;
  global_fieldref_cnt += frefs_cnt;
}

void update_class_stats(DexClass* clazz) {
  int cnt_smeths = 0;
  for (auto const m : clazz->get_dmethods()) {
    if (is_static(m)) {
      cnt_smeths++;
    }
  }
  global_smeth_cnt += cnt_smeths;
  global_dmeth_cnt += clazz->get_dmethods().size();
  global_vmeth_cnt += clazz->get_vmethods().size();
}

/*
 * Sanity check: did gather_refs return all the refs that ultimately ended up
 * in the dex?
 */
void check_refs_count(const dex_emit_tracker& det, const DexClasses& dc) {
  std::vector<DexMethodRef*> mrefs;
  for (DexClass* cls : dc) {
    cls->gather_methods(mrefs);
  }
  std::unordered_set<DexMethodRef*> mrefs_set(mrefs.begin(), mrefs.end());
  if (mrefs_set.size() > det.mrefs.size()) {
    for (DexMethodRef* mr : mrefs_set) {
      if (!det.mrefs.count(mr)) {
        TRACE(IDEX, 1,
              "WARNING: Could not find %s in predicted mrefs set\n",
              SHOW(mr));
      }
    }
  }

  std::vector<DexFieldRef*> frefs;
  for (DexClass* cls : dc) {
    cls->gather_fields(frefs);
  }
  std::unordered_set<DexFieldRef*> frefs_set(frefs.begin(), frefs.end());
  if (frefs_set.size() > det.frefs.size()) {
    for (auto* fr : frefs_set) {
      if (!det.frefs.count(fr)) {
        TRACE(IDEX, 1,
              "WARNING: Could not find %s in predicted frefs set\n",
              SHOW(fr));
      }
    }
  }

  // print out stats
  TRACE(IDEX, 1,
        "terminating dex at classes %lu, lin alloc %d:%d, mrefs %lu:%lu:%d, "
        "frefs %lu:%lu:%d\n",
        det.outs.size(),
        det.la_size,
        linear_alloc_limit,
        det.mrefs.size(),
        mrefs_set.size(),
        kMaxMethodRefs,
        det.frefs.size(),
        frefs_set.size(),
        kMaxFieldRefs);
}

void flush_out_dex(InterDexPass* pass,
                   dex_emit_tracker& det,
                   DexClassesVector& outdex) {
  DexClasses dc(det.outs.size());
  for (size_t i = 0; i < det.outs.size(); i++) {
    auto cls = det.outs[i];
    TRACE(IDEX, 4, "IDEX: Emitting class :: %s\n", SHOW(cls));
    dc.at(i) = cls;
  }
  for (auto& plugin : pass->m_plugins) {
    auto add_classes = plugin->additional_classes(outdex, det.outs);
    for (auto add_class : add_classes) {
      TRACE(IDEX, 4, "IDEX: Emitting plugin-generated class :: %s\n",
            SHOW(add_class));
    }
    dc.insert(dc.end(), add_classes.begin(), add_classes.end());
  }
  check_refs_count(det, dc);

  outdex.emplace_back(std::move(dc));

  update_dex_stats(det.outs.size(), det.mrefs.size(), det.frefs.size());
  det.start_new_dex();
}

void flush_out_secondary(InterDexPass* pass,
                         dex_emit_tracker& det,
                         DexClassesVector& outdex,
                         const std::shared_ptr<FILE*>& mixed_mode) {
  // don't emit dex if we don't have any classes
  if (!det.outs.size()) {
    return;
  }
  // Find the Canary class and add it in.
  if (emit_canaries) {
    int dexnum = ((int)outdex.size());
    char buf[kCanaryClassBufsize];
    always_assert_log(dexnum <= kMaxDexNum,
                      "Bailing, Max dex number surpassed %d\n", dexnum);
    snprintf(buf, sizeof(buf), kCanaryClassFormat, dexnum);
    std::string canaryname(buf);
    auto it = det.clookup.find(canaryname);
    if (it == det.clookup.end()) {
      TRACE(IDEX, 2, "Warning, no canary class %s found\n", buf);
      auto canary_type = DexType::make_type(canaryname.c_str());
      auto canary_cls = type_class(canary_type);
      if (canary_cls == nullptr) {
        // class doesn't exist, we have to create it
        // this can happen if we grow the number of dexes
        ClassCreator cc(canary_type);
        cc.set_access(ACC_PUBLIC | ACC_INTERFACE | ACC_ABSTRACT);
        cc.set_super(get_object_type());
        canary_cls = cc.create();
      }
      det.outs.push_back(canary_cls);
    } else {
      auto clazz = it->second;
      det.outs.push_back(clazz);
    }
    if (mixed_mode != nullptr) {
      auto mixed_mode_fh = FileHandle(*mixed_mode);
      mixed_mode_fh.seek_end();
      write_str(mixed_mode_fh, canaryname + "\n");
    }
  }

  // Now emit our outs list...
  flush_out_dex(pass, det, outdex);
}

void flush_out_secondary(InterDexPass* pass,
                         dex_emit_tracker& det,
                         DexClassesVector& outdex) {
  flush_out_secondary(pass, det, outdex, nullptr);
}

void flush_out_mixed_mode_dex(InterDexPass* pass,
                              ApkManager& apk_manager,
                              dex_emit_tracker& det,
                              DexClassesVector& outdex) {
  auto mix_mode = apk_manager.new_asset_file("mixed_mode.txt");
  flush_out_secondary(pass, det, outdex, mix_mode);
  *mix_mode = nullptr;
}

bool is_canary(DexClass* clazz) {
  const char* cname = clazz->get_type()->get_name()->c_str();
  return strncmp(cname, kCanaryPrefix, sizeof(kCanaryPrefix) - 1) == 0;
}

struct PenaltyPattern {
  const char* suffix;
  unsigned penalty;

  PenaltyPattern(const char* str, unsigned pen)
    : suffix(str),
      penalty(pen)
  {}
};

const PenaltyPattern kPatterns[] = {
  { "Layout;", 1500 },
  { "View;", 1500 },
  { "ViewGroup;", 1800 },
  { "Activity;", 1500 },
};

const unsigned kObjectVtable = 48;
const unsigned kMethodSize = 52;
const unsigned kInstanceFieldSize = 16;
const unsigned kVtableSlotSize = 4;

bool matches_penalty(const char* str, unsigned& penalty) {
  for (auto const& pattern : kPatterns) {
    if (ends_with(str, pattern.suffix)) {
      penalty = pattern.penalty;
      return true;
    }
  }
  return false;
}

/**
 * Estimates the linear alloc space consumed by the class at runtime.
 */
unsigned estimate_linear_alloc(const DexClass* clazz) {
  unsigned lasize = 0;
  // VTable guesstimate. Technically we could do better here, but only so much.
  // Try to stay bug-compatible with DalvikStatsTool.
  if (!is_interface(clazz)) {
    unsigned vtablePenalty = kObjectVtable;
    if (!matches_penalty(clazz->get_type()->get_name()->c_str(), vtablePenalty)
        && clazz->get_super_class() != nullptr) {
      /* what?, we could be redexing object some day... :) */
      matches_penalty(
          clazz->get_super_class()->get_name()->c_str(), vtablePenalty);
    }
    lasize += vtablePenalty;
    lasize += clazz->get_vmethods().size() * kVtableSlotSize;
  }
  /* Dmethods... */
  lasize += clazz->get_dmethods().size() * kMethodSize;
  /* Vmethods... */
  lasize += clazz->get_vmethods().size() * kMethodSize;
  /* Instance Fields */
  lasize += clazz->get_ifields().size() * kInstanceFieldSize;
  return lasize;
}

bool should_skip_class(InterDexPass* pass, DexClass* clazz) {
  for (const auto& plugin : pass->m_plugins) {
    if (plugin->should_skip_class(clazz)) {
      return true;
    }
  }
  return false;
}

bool is_mixed_mode_class(DexClass* clazz) {
  return mixed_mode_classes.count(clazz);
}

/*
 * Try and fit :clazz into the last dex in the :outdex vector. If that would
 * result in excessive member refs, start a new dex, putting :clazz in there.
 */
void emit_class(InterDexPass* pass,
                dex_emit_tracker& det,
                DexClassesVector& outdex,
                DexClass* clazz,
                bool is_primary,
                bool check_if_skip = true) {
  if (det.emitted.count(clazz) != 0 || is_canary(clazz)) {
    return;
  }
  if (check_if_skip && should_skip_class(pass, clazz)) {
    TRACE(IDEX, 3, "IDEX: Skipping class :: %s\n", SHOW(clazz));
    return;
  }
  if (!is_primary && check_if_skip && is_mixed_mode_class(clazz)) {
    TRACE(IDEX, 2, "IDEX: Skipping mixed mode class :: %s\n", SHOW(clazz));
    return;
  }

  unsigned laclazz = estimate_linear_alloc(clazz);

  // Calculate the extra method and field refs that we would need to add to
  // the current dex if we defined :clazz in it.
  mrefs_t clazz_mrefs;
  frefs_t clazz_frefs;
  gather_refs(pass, clazz, &clazz_mrefs, &clazz_frefs);
  auto extra_mrefs = set_difference(clazz_mrefs, det.mrefs);
  auto extra_frefs = set_difference(clazz_frefs, det.frefs);

  // If those extra refs would cause use to overflow, start a new dex.
  if ((det.la_size + laclazz) > linear_alloc_limit ||
      // XXX(jezng): shouldn't this >= be > instead?
      det.mrefs.size() + extra_mrefs.size() >= kMaxMethodRefs ||
      det.frefs.size() + extra_frefs.size() >= kMaxFieldRefs) {
    // Emit out list
    always_assert_log(!is_primary,
                      "would have to do an early flush on the primary dex\n"
                      "la %d:%d , mrefs %lu:%d frefs %lu:%d\n",
                      det.la_size + laclazz,
                      linear_alloc_limit,
                      det.mrefs.size() + extra_mrefs.size(),
                      kMaxMethodRefs,
                      det.frefs.size() + extra_frefs.size(),
                      kMaxFieldRefs);
    flush_out_secondary(pass, det, outdex);
  }

  det.mrefs.insert(clazz_mrefs.begin(), clazz_mrefs.end());
  det.frefs.insert(clazz_frefs.begin(), clazz_frefs.end());
  det.la_size += laclazz;
  det.outs.push_back(clazz);
  det.emitted.insert(clazz);
  update_class_stats(clazz);
}

void emit_class(InterDexPass* pass,
                dex_emit_tracker& det,
                DexClassesVector& outdex,
                DexClass* clazz) {
  emit_class(pass, det, outdex, clazz, false);
}

std::unordered_set<const DexClass*> find_unrefenced_coldstart_classes(
    const Scope& scope,
    dex_emit_tracker& det,
    const std::vector<std::string>& interdexorder,
    bool static_prune_classes) {
  int old_no_ref = -1;
  int new_no_ref = 0;
  std::unordered_set<DexClass*> coldstart_classes;
  std::unordered_set<const DexClass*> cold_cold_references;
  std::unordered_set<const DexClass*> unreferenced_classes;
  Scope input_scope = scope;

  // don't do analysis if we're not going doing pruning
  if (!static_prune_classes) {
    return unreferenced_classes;
  }

  for (auto const& class_string : interdexorder) {
    if (det.clookup.count(class_string)) {
      coldstart_classes.insert(det.clookup[class_string]);
    }
  }

  while (old_no_ref != new_no_ref) {
    old_no_ref = new_no_ref;
    new_no_ref = 0;
    cold_cold_references.clear();
    walk::code(
      input_scope,
      [&](DexMethod* meth) {
        return coldstart_classes.count(type_class(meth->get_class())) > 0;
      },
      [&](DexMethod* meth, const IRCode& code) {
        auto base_cls = type_class(meth->get_class());
        for (auto& mie : InstructionIterable(meth->get_code())) {
          auto inst = mie.insn;
          DexClass* called_cls = nullptr;
          if (inst->has_method()) {
            called_cls = type_class(inst->get_method()->get_class());
          } else if (inst->has_field()) {
            called_cls = type_class(inst->get_field()->get_class());
          } else if (inst->has_type()) {
            called_cls = type_class(inst->get_type());
          }
          if (called_cls != nullptr && base_cls != called_cls &&
              coldstart_classes.count(called_cls) > 0) {
            cold_cold_references.insert(called_cls);
          }
        }
      }
    );
    for (const auto& cls: scope) {
      // make sure we don't drop classes which might be called from native code
      if (!can_rename(cls)) {
        cold_cold_references.insert(cls);
      }
    }
    // get all classes in the reference set, even if they are not referenced by
    // opcodes directly
    for (const auto& cls: input_scope) {
      if (cold_cold_references.count(cls)) {
        std::vector<DexType*> types;
        cls->gather_types(types);
        for (const auto& type: types) {
          auto ref_cls = type_class(type);
          cold_cold_references.insert(ref_cls);
        }
      }
    }
    Scope output_scope;
    for (auto& cls : coldstart_classes) {
      if (can_rename(cls) && cold_cold_references.count(cls) == 0) {
        new_no_ref++;
        unreferenced_classes.insert(cls);
      } else {
        output_scope.push_back(cls);
      }
    }
    TRACE(IDEX, 1, "found %d classes in coldstart with no references\n",
          new_no_ref);
    input_scope = output_scope;
  }
  return unreferenced_classes;
}

void get_mix_mode_classes(const std::string& mixed_mode_classes_file) {
  std::ifstream input(mixed_mode_classes_file.c_str(), std::ifstream::in);
  if (!input) {
    TRACE(IDEX, 1, "Mixed mode class file: %s : not found\n",
          mixed_mode_classes_file.c_str());
    return;
  }
  std::string class_name;
  while (input >> class_name) {
    auto type = DexType::get_type(class_name.c_str());
    if (!type) {
      TRACE(IDEX, 2, "Couldn't find DexType for mixed mode class: %s\n",
            class_name.c_str());
      continue;
    }
    auto cls = type_class(type);
    if (!cls) {
      TRACE(IDEX, 2, "Couldn't find DexClass for mixed mode class: %s\n",
            class_name.c_str());
      continue;
    }
    if (mixed_mode_classes.count(cls)) {
      TRACE(IDEX, 1, "Duplicate classes found in mixed mode list\n");
      exit(1);
    }
    TRACE(IDEX, 2, "Adding %s in mixed mode list\n", SHOW(cls));
    mixed_mode_classes.emplace(cls);
  }
  input.close();
}

void get_mix_mode_classes(const std::string& mixed_mode_classes_file,
                          const DexClassesVector& dexen) {
  // If we have the list of the classes defined, use it.
  if (!mixed_mode_classes_file.empty()) {
    get_mix_mode_classes(mixed_mode_classes_file);
    return;
  }

  // Otherwise, check for classes that have the mix mode flag set.
  for (const auto& dex : dexen) {
    for (const auto& cls : dex) {
      if (cls->rstate.has_mix_mode()) {
        TRACE(IDEX, 4, "Adding class %s to the scroll list\n", SHOW(cls));
        mixed_mode_classes.emplace(cls);
      }
    }
  }
}

void emit_mixed_mode_classes(const std::vector<std::string>& interdexorder,
                             InterDexPass* pass,
                             dex_emit_tracker& det,
                             DexClassesVector& outdex,
                             ApkManager& apk_manager,
                             bool can_touch_interdex_order) {
  // Emit mix mode classes in a separate dex.
  // We respect the order of the classes in the interdexorder,
  //   for the mixed mode classes that it contains.

  // NOTE: When we got here, we would have removed the coldstart
  //       mixed mode classes, if we couldn't touch them.
  //       The only classes that might still be in the mixed_mode_cls
  //       set would be the extended ones, which we will remove
  //       if needed.
  for (auto& elem : interdexorder) {
    auto det_it = det.clookup.find(elem);
    if (det_it == det.clookup.end()) {
      continue;
    }

    auto clazz = det_it->second;
    if (mixed_mode_classes.count(clazz)) {
      if (can_touch_interdex_order) {
        TRACE(IDEX, 2, " Emitting mixed mode class, that is also in the "
              "interdex list: %s \n", SHOW(clazz));
        emit_class(pass, det, outdex, clazz, false, false);
      }
      mixed_mode_classes.erase(clazz);
    }
  }

  for (const auto& clazz : mixed_mode_classes) {
    const auto& cls_name = clazz->get_name()->str();
    if (!det.clookup.count(cls_name)) {
      TRACE(IDEX, 2, "Ignoring mixed mode class %s as it is not found in "
            "dexes\n", cls_name.c_str());
      continue;
    }
    TRACE(IDEX, 2, " Emitting mixed mode class: %s \n", cls_name.c_str());
    emit_class(pass, det, outdex, clazz, false, false);
  }

  // Flush the mixed mode classes
  if (det.outs.size()) {
    flush_out_mixed_mode_dex(pass, apk_manager, det, outdex);
  }

  // Clearing up the mixed mode classes.
  mixed_mode_classes.clear();
}

DexClassesVector run_interdex(InterDexPass* pass,
                              ApkManager& apk_manager,
                              const DexClassesVector& dexen,
                              const std::string& mixed_mode_classes_file,
                              ConfigFiles& cfg,
                              bool allow_cutting_off_dex,
                              bool static_prune_classes,
                              bool normal_primary_dex,
                              bool can_touch_coldstart_cls,
                              bool can_touch_coldstart_extended_cls) {

  global_dmeth_cnt = 0;
  global_smeth_cnt = 0;
  global_vmeth_cnt = 0;
  global_methref_cnt = 0;
  global_fieldref_cnt = 0;
  global_cls_cnt = 0;

  cls_skipped_in_primary = 0;
  cls_skipped_in_secondary = 0;

  auto interdexorder = cfg.get_coldstart_classes();
  get_mix_mode_classes(mixed_mode_classes_file, dexen);

  dex_emit_tracker det;
  for (auto const& dex : dexen) {
    for (auto const& clazz : dex) {
      std::string clzname(clazz->get_type()->get_name()->c_str());
      det.clookup[clzname] = clazz;
      TRACE(IDEX, 2, "Adding class to dex.clookup %s , %s\n", clzname.c_str(), SHOW(clazz));
    }
  }

  auto scope = build_class_scope(dexen);

  auto unreferenced_classes = find_unrefenced_coldstart_classes(
      scope,
      det,
      interdexorder,
      static_prune_classes);

  DexClassesVector outdex;
  auto const& primary_dex = dexen[0];

  // We have a bunch of special logic for the primary dex which we only use if
  // we can't touch the primary dex.
  if (!normal_primary_dex) {
    // build a separate lookup table for the primary dex, since we have to make
    // sure we keep all classes in the same dex
    dex_emit_tracker primary_det;
    for (auto const& clazz : primary_dex) {
      std::string clzname(clazz->get_type()->get_name()->c_str());
      primary_det.clookup[clzname] = clazz;
    }

    // First emit just the primary dex, but sort it according to interdex order
    auto coldstart_classes_in_primary = 0;
    // first add the classes in the interdex list
    for (auto& entry : interdexorder) {
      auto it = primary_det.clookup.find(entry);
      if (it == primary_det.clookup.end()) {
        TRACE(IDEX, 4, "No such entry %s\n", entry.c_str());
        continue;
      }
      auto clazz = it->second;
      if (unreferenced_classes.count(clazz)) {
        TRACE(IDEX, 3, "%s no longer linked to coldstart set.\n", SHOW(clazz));
        cls_skipped_in_primary++;
        continue;
      }

      emit_class(pass, primary_det, outdex, clazz, true);
      coldstart_classes_in_primary++;
    }
    // now add the rest
    for (auto const& clazz : primary_dex) {
      emit_class(pass, primary_det, outdex, clazz, true);
    }
    TRACE(IDEX, 1,
        "%d out of %lu classes in primary dex in interdex list\n",
        coldstart_classes_in_primary,
        primary_det.outs.size());
    flush_out_dex(pass, primary_det, outdex);
    // record the primary dex classes in the main emit tracker,
    // so we don't emit those classes again. *cough*
    for (auto const& clazz : primary_dex) {
      det.emitted.insert(clazz);
    }
  }

  // If we have end-markers, we use them to demarcate the end of the
  // cold-start set.  Otherwise, we calculate it on the basis of the
  // whole list.
  bool end_markers_present = false;

  // NOTE: If primary dex is treated as a normal dex, we are going to modify
  //       it too, based on cold start classes.
  if (normal_primary_dex && interdexorder.size() > 0) {
    // We also need to respect the primary dex classes.
    // For all primary dex classes that are in the interdex order before
    // any DexEndMarker, we keep it at that position. Otherwise, we add it to
    // the head of the list.
    std::string first_end_marker_str("DexEndMarker0.class");
    auto first_end_marker_it = std::find(
        interdexorder.begin(), interdexorder.end(), first_end_marker_str);
    if (first_end_marker_it == interdexorder.end()) {
      TRACE(IDEX, 3, "Couldn't find first dex end marker.\n");
    }

    std::vector<std::string> not_already_included;
    for (const auto& pclass : primary_dex) {
      const std::string& pclass_str = pclass->get_name()->str();
      auto pclass_it = std::find(
          interdexorder.begin(), interdexorder.end(), pclass_str);
      if (pclass_it == interdexorder.end() || pclass_it > first_end_marker_it) {
        TRACE(IDEX, 4, "Class %s is not in the interdex order.\n",
              pclass_str.c_str());
        not_already_included.push_back(std::string(pclass->c_str()));
      } else {
        TRACE(IDEX, 4, "Class %s is in the interdex order. "
              "No change required.\n", pclass_str.c_str());
      }
    }
    interdexorder.insert(interdexorder.begin(),
                         not_already_included.begin(),
                         not_already_included.end());
  }


  // Last end market delimits where the whole coldstart set ends
  // and the extended coldstart set begins.
  std::string last_end_marker_str("LDexEndMarker1;");
  auto last_end_marker_it = std::find(
      interdexorder.begin(), interdexorder.end(), last_end_marker_str);

  for (auto it_interdex = interdexorder.begin();
       it_interdex != interdexorder.end(); ++it_interdex) {
    auto& entry = *it_interdex;
    auto it = det.clookup.find(entry);
    if (it == det.clookup.end()) {
      TRACE(IDEX, 4, "No such entry %s\n", entry.c_str());
      if (entry.find("DexEndMarker") != std::string::npos) {
        TRACE(IDEX, 1, "Terminating dex due to DexEndMarker\n");
        flush_out_secondary(pass, det, outdex);
        cold_start_set_dex_count = outdex.size();
        end_markers_present = true;

        if (last_end_marker_it == it_interdex &&
            mixed_mode_classes.size() > 0) {
          TRACE(IDEX, 3, "Emitting the mixed mode dex between the coldstart "
                "set and the extended set of classes.\n");
          bool can_touch_interdex_order = can_touch_coldstart_cls ||
                                          can_touch_coldstart_extended_cls;
          emit_mixed_mode_classes(
              interdexorder, pass, det, outdex, apk_manager,
              can_touch_interdex_order);
        }
      }
      continue;
    }

    auto clazz = it->second;

    // If we can't touch coldstart classes, simply remove the class
    // from the mix mode class list. Otherwise, we will end up moving
    // the class in the mixed mode dex.
    if (!can_touch_coldstart_cls && mixed_mode_classes.count(clazz)) {
      if (last_end_marker_it > it_interdex) {
        TRACE(IDEX, 2, "%s is part of coldstart classes. Removing it from the "
              "list of mix mode classes\n", SHOW(clazz));
        mixed_mode_classes.erase(clazz);
      } else if (!can_touch_coldstart_extended_cls) {
        always_assert_log(false, "We shouldn't get here since we cleared "
                          "it up when emitting the mixed mode dex!\n");
      }
    }

    if (unreferenced_classes.count(clazz)) {
      TRACE(IDEX, 3, "%s no longer linked to coldstart set.\n", SHOW(clazz));
      cls_skipped_in_secondary++;
      continue;
    }

    emit_class(pass, det, outdex, clazz);
  }

  if (last_end_marker_it == interdexorder.end()) {
    // If we got here, we didn't find the delimiter -> emitting the mixed mode
    // classes here.
    TRACE(IDEX, 3, "Emitting the mixed mode dex after the interdex order.\n");
    bool can_touch_interdex_order = can_touch_coldstart_cls ||
                                    can_touch_coldstart_extended_cls;
    emit_mixed_mode_classes(interdexorder, pass, det, outdex,
                            apk_manager, can_touch_interdex_order);
  }

  // Now emit the classes we omitted from the original coldstart set
  for (auto& entry : interdexorder) {
    auto it = det.clookup.find(entry);
    if (it == det.clookup.end()) {
      TRACE(IDEX, 4, "No such entry %s\n", entry.c_str());
      continue;
    }
    auto clazz = it->second;
    if (unreferenced_classes.count(clazz)) {
      emit_class(pass, det, outdex, clazz);
    }
  }

  if (!end_markers_present) {
    // -1 because we're not counting the primary dex
    cold_start_set_dex_count = outdex.size();
  }

  // Now emit the classes that weren't specified in the head or primary list.
  for (auto clazz : scope) {
    emit_class(pass, det, outdex, clazz);
  }
  for (const auto& plugin : pass->m_plugins) {
    auto add_classes = plugin->leftover_classes();
    for (auto add_class : add_classes) {
      TRACE(IDEX,
            4,
            "IDEX: Emitting plugin generated leftover class :: %s\n",
            SHOW(add_class));
      emit_class(
          pass,
          det,
          outdex,
          add_class,
          false, /* not primary */
          false /* shouldn't skip */);
    }
  }

  // Finally, emit the "left-over" det.outs
  if (det.outs.size()) {
    flush_out_secondary(pass, det, outdex);
  }

  TRACE(IDEX, 1, "InterDex secondary dex count %d\n", (int)(outdex.size() - 1));
  TRACE(IDEX, 1,
        "global stats: %lu mrefs, %lu frefs, %lu cls, %lu dmeth, %lu smeth, "
        "%lu vmeth\n",
        global_methref_cnt,
        global_fieldref_cnt,
        global_cls_cnt,
        global_dmeth_cnt,
        global_smeth_cnt,
        global_vmeth_cnt);
  TRACE(IDEX, 1,
    "removed %d classes from coldstart list in primary dex, \
%d in secondary dexes due to static analysis\n",
    cls_skipped_in_primary,
    cls_skipped_in_secondary);
  return outdex;
}

} // End namespace


void InterDexPass::run_pass(DexClassesVector& dexen,
                            Scope& original_scope,
                            ConfigFiles& cfg,
                            PassManager& mgr) {
  InterDexRegistry* registry = static_cast<InterDexRegistry*>(
      PluginRegistry::get().pass_registry(INTERDEX_PASS_NAME));
  m_plugins = registry->create_plugins();
  for (const auto& plugin : m_plugins) {
    plugin->configure(original_scope, cfg);
  }
  emit_canaries = m_emit_canaries;
  linear_alloc_limit = m_linear_alloc_limit;
  dexen = run_interdex(this, mgr.apk_manager(), dexen, m_mixed_mode_classes_file,
                       cfg, true, m_static_prune, m_normal_primary_dex,
                       m_can_touch_coldstart_cls,
                       m_can_touch_coldstart_extended_cls);
  for (const auto& plugin : m_plugins) {
    plugin->cleanup(original_scope);
  }
  mgr.incr_metric(METRIC_COLD_START_SET_DEX_COUNT, cold_start_set_dex_count);

  m_plugins.clear();
}

void InterDexPass::run_pass(DexStoresVector& stores,
                            ConfigFiles& cfg,
                            PassManager& mgr) {
  if (mgr.no_proguard_rules()) {
    TRACE(IDEX, 1, "InterDexPass not run because no ProGuard configuration was provided.");
    return;
  }
  auto original_scope = build_class_scope(stores);
  for (auto& store : stores) {
    if (store.get_name() == "classes") {
      run_pass(store.get_dexen(), original_scope, cfg, mgr);
    }
  }
}

static InterDexPass s_pass;

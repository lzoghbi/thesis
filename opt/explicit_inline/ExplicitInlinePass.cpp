    #include "IRCode.h"
    #include "DexUtil.h"
    #include "Walkers.h"
    #include "Inliner.h"
    #include "DexClass.h"
    #include "Creators.h"
    #include "VirtualScope.h"
    #include "ClassHierarchy.h"
    #include "ExplicitInlinePass.h"

struct Location;

using MethodImplsStr = std::unordered_set<std::string>;
using MethodsToInline = std::unordered_map<std::string, MethodImplsStr>; 

void parse_file(std::string filename, MethodsToInline& imethods){
  std::ifstream file(filename);

  if(file.is_open()){
    while(!file.eof()){
      std::string method, callsite;
      std::getline (file, method, '\t');
      std::getline (file, callsite, '\n');

      if(!method.empty() && !callsite.empty()){
        if(imethods.find(callsite) == imethods.end()){
          MethodImplsStr method_impls;
          imethods.insert(std::make_pair(callsite, method_impls));
        }
        imethods[callsite].insert(method);
      }
    }
  }
  else{ 
    std::cout << "Could not open file " << filename << std::endl;
  }
}

DexType* get_callee_type(std::string callee_str){
  std::istringstream iss(callee_str);
  std::string cls_name;

  std::getline(iss, cls_name, '.');

  return DexType::get_type(cls_name.c_str());
}

std::string get_caller_str(std::string caller_name){    
  char last_char = caller_name.c_str()[caller_name.size()-1];
  while(isdigit(last_char) || last_char == '/'){
    caller_name = caller_name.substr(0, caller_name.size()-1);
    last_char = caller_name.c_str()[caller_name.size()-1];
  }

  return caller_name;
}

uint16_t get_insn_index(std::string callsite){
  size_t last_index = callsite.find_last_not_of("0123456789");
  std::string insn_index = callsite.substr(last_index + 1);

  return atoi(insn_index.c_str());
}

void ExplicitInlinePass::run_pass(DexStoresVector& stores, 
                                  ConfigFiles& cfg, 
                                  PassManager& mgr) {

  MethodsToInline imethodsStr;
  DexMethod *caller, *callee;
  parse_file("imethods.txt", imethodsStr);
  ClassHierarchy ch = build_type_hierarchy(scope);

  for(auto map_entry : imethodsStr){
    std::string caller_str = get_caller_str(map_entry.first);
    uint16_t insn_index    = get_insn_index(map_entry.first);
    caller = (DexMethod*) DexMethod::get_method(caller_str);

    IRList::iterator it;
    IRInstruction *insn;
    uint16_t idx = 0, this_reg;
    IRCode *caller_code = caller->get_code();
    auto caller_mc = new MethodCreator(caller);
    for(auto& mie : InstructionIterable(caller_code)){
      if(idx++ == insn_index){
        insn = mie.insn;
        this_reg = insn->src(0);
        it = caller_code->iterator_to(mie);
        break;
      }
    }

    IRList::iterator false_block_it, true_block_it = it;
    MethodImplsStr method_impls = map_entry.second;
    if(method_impls.size() > 1){
      for(auto impl : method_impls){
        callee = (DexMethod*) DexMethod::get_method(impl);
        DexType* type = get_callee_type(impl);
        auto dst_location = caller_code->get_registers_size();
        caller_code->set_registers_size(dst_location+1);


        //instance of
        auto instance_of_insn = new IRInstruction(OPCODE_INSTANCE_OF);
        auto move_res = new IRInstruction(IOPCODE_MOVE_RESULT_PSEUDO);
        instance_of_insn->set_src(0, this_reg);
        instance_of_insn->set_type(type);
        move_res->set_dest(dst_location);
        auto instanceof_it = caller_code->insert_after(true_block_it, instance_of_insn);
        instanceof_it = caller_code->insert_after(instanceof_it, move_res);


        //if block
        auto if_insn = new IRInstruction(OPCODE_IF_EQZ);
        if_insn->set_src(0, dst_location);
        auto if_entry  = new MethodItemEntry(if_insn);
        false_block_it = caller_code->insert_after(instanceof_it, *if_entry);
        
        //else goto
        auto goto_insn  = new IRInstruction(OPCODE_GOTO);
        auto goto_entry = new MethodItemEntry(goto_insn);
        auto goto_it = caller_code->insert_after(std::prev(caller_code->end()),
                                                 *goto_entry);

        // main block
        auto main_bt    = new BranchTarget(goto_entry);
        auto main_entry = new MethodItemEntry(main_bt);
        auto main_block = caller_code->insert_after(goto_it, *main_entry);

        // else block
        auto else_bt  = new BranchTarget(if_entry);
        auto eb_entry = new MethodItemEntry(else_bt);
        true_block_it = caller_code->insert_after(goto_it, *eb_entry);


        auto invoke_insn = new IRInstruction(OPCODE_INVOKE_VIRTUAL);
        invoke_insn->set_method(callee)->set_arg_word_count(insn->arg_word_count());

        for (uint16_t i = 0; i < insn->srcs().size(); i++) {
          invoke_insn->set_src(i, insn->srcs().at(i));
        }
        if (false_block_it == caller_code->end()) {
          caller_code->push_back(invoke_insn);
          std::prev(caller_code->end());
        } else {
          false_block_it = caller_code->insert_after(false_block_it, invoke_insn);
        }

        // inliner::inline_method(caller->get_code(),
        //                        callee->get_code(),
        //                        false_block_it);      
      }

      // caller_code->erase(it);
      caller_mc->create();   
    
    }
    else{
      auto impl_it = method_impls.begin();
      callee = (DexMethod*) DexMethod::get_method(*impl_it);
      inliner::inline_method(caller->get_code(), callee->get_code(), it);
      
    }
  }           
  std::cout << SHOW(caller->get_code()) << std::endl;
  std::cout << "Explicit inline pass done." << std::endl;
}

static ExplicitInlinePass s_pass;
// Copyright (c) 2016, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "vm/globals.h"  // Needed here to get TARGET_ARCH_DBC.
#if defined(TARGET_ARCH_DBC)

#include "vm/flow_graph_compiler.h"

#include "vm/ast_printer.h"
#include "vm/compiler.h"
#include "vm/cpu.h"
#include "vm/dart_entry.h"
#include "vm/deopt_instructions.h"
#include "vm/il_printer.h"
#include "vm/instructions.h"
#include "vm/locations.h"
#include "vm/object_store.h"
#include "vm/parser.h"
#include "vm/stack_frame.h"
#include "vm/stub_code.h"
#include "vm/symbols.h"
#include "vm/verified_memory.h"

namespace dart {

DEFINE_FLAG(bool, trap_on_deoptimization, false, "Trap on deoptimization.");
DEFINE_FLAG(bool, unbox_mints, true, "Optimize 64-bit integer arithmetic.");
DEFINE_FLAG(bool, unbox_doubles, true, "Optimize double arithmetic.");
DECLARE_FLAG(bool, enable_simd_inline);
DECLARE_FLAG(bool, use_megamorphic_stub);
DECLARE_FLAG(charp, optimization_filter);

void MegamorphicSlowPath::EmitNativeCode(FlowGraphCompiler* compiler) {
  UNIMPLEMENTED();
}


FlowGraphCompiler::~FlowGraphCompiler() {
  // BlockInfos are zone-allocated, so their destructors are not called.
  // Verify the labels explicitly here.
  for (int i = 0; i < block_info_.length(); ++i) {
    ASSERT(!block_info_[i]->jump_label()->IsLinked());
  }
}


bool FlowGraphCompiler::SupportsUnboxedDoubles() {
  return false;
}


bool FlowGraphCompiler::SupportsUnboxedMints() {
  return false;
}


bool FlowGraphCompiler::SupportsUnboxedSimd128() {
  return false;
}


bool FlowGraphCompiler::SupportsSinCos() {
  return false;
}


bool FlowGraphCompiler::SupportsHardwareDivision() {
  return true;
}


bool FlowGraphCompiler::CanConvertUnboxedMintToDouble() {
  return false;
}


void FlowGraphCompiler::EnterIntrinsicMode() {
  ASSERT(!intrinsic_mode());
  intrinsic_mode_ = true;
}


void FlowGraphCompiler::ExitIntrinsicMode() {
  ASSERT(intrinsic_mode());
  intrinsic_mode_ = false;
}


RawTypedData* CompilerDeoptInfo::CreateDeoptInfo(FlowGraphCompiler* compiler,
                                                 DeoptInfoBuilder* builder,
                                                 const Array& deopt_table) {
  if (deopt_env_ == NULL) {
    ++builder->current_info_number_;
    return TypedData::null();
  }

  intptr_t stack_height = compiler->StackSize();
  AllocateIncomingParametersRecursive(deopt_env_, &stack_height);

  intptr_t slot_ix = 0;
  Environment* current = deopt_env_;

  // Emit all kMaterializeObject instructions describing objects to be
  // materialized on the deoptimization as a prefix to the deoptimization info.
  EmitMaterializations(deopt_env_, builder);

  // The real frame starts here.
  builder->MarkFrameStart();

  Zone* zone = compiler->zone();

  builder->AddCallerFp(slot_ix++);
  builder->AddReturnAddress(current->function(), deopt_id(), slot_ix++);
  builder->AddPcMarker(Function::ZoneHandle(zone), slot_ix++);
  builder->AddConstant(Function::ZoneHandle(zone), slot_ix++);

  // Emit all values that are needed for materialization as a part of the
  // expression stack for the bottom-most frame. This guarantees that GC
  // will be able to find them during materialization.
  slot_ix = builder->EmitMaterializationArguments(slot_ix);

  // For the innermost environment, set outgoing arguments and the locals.
  for (intptr_t i = current->Length() - 1;
       i >= current->fixed_parameter_count();
       i--) {
    builder->AddCopy(current->ValueAt(i), current->LocationAt(i), slot_ix++);
  }

  builder->AddCallerFp(slot_ix++);

  Environment* previous = current;
  current = current->outer();
  while (current != NULL) {
    // For any outer environment the deopt id is that of the call instruction
    // which is recorded in the outer environment.
    builder->AddReturnAddress(
        current->function(),
        Thread::ToDeoptAfter(current->deopt_id()),
        slot_ix++);

    builder->AddPcMarker(previous->function(), slot_ix++);
    builder->AddConstant(previous->function(), slot_ix++);

    // The values of outgoing arguments can be changed from the inlined call so
    // we must read them from the previous environment.
    for (intptr_t i = previous->fixed_parameter_count() - 1; i >= 0; i--) {
      builder->AddCopy(previous->ValueAt(i),
                       previous->LocationAt(i),
                       slot_ix++);
    }

    // Set the locals, note that outgoing arguments are not in the environment.
    for (intptr_t i = current->Length() - 1;
         i >= current->fixed_parameter_count();
         i--) {
      builder->AddCopy(current->ValueAt(i),
                       current->LocationAt(i),
                       slot_ix++);
    }

    builder->AddCallerFp(slot_ix++);

    // Iterate on the outer environment.
    previous = current;
    current = current->outer();
  }
  // The previous pointer is now the outermost environment.
  ASSERT(previous != NULL);

  // For the outermost environment, set caller PC.
  builder->AddCallerPc(slot_ix++);

  builder->AddPcMarker(previous->function(), slot_ix++);
  builder->AddConstant(previous->function(), slot_ix++);


  // For the outermost environment, set the incoming arguments.
  for (intptr_t i = previous->fixed_parameter_count() - 1; i >= 0; i--) {
    builder->AddCopy(previous->ValueAt(i), previous->LocationAt(i), slot_ix++);
  }

  return builder->CreateDeoptInfo(deopt_table);
}


void FlowGraphCompiler::RecordAfterCall(Instruction* instr) {
  RecordSafepoint(instr->locs());
  // Marks either the continuation point in unoptimized code or the
  // deoptimization point in optimized code, after call.
  const intptr_t deopt_id_after = Thread::ToDeoptAfter(instr->deopt_id());
  if (is_optimizing()) {
    // Return/ReturnTOS instruction drops incoming arguments so
    // we have to drop outgoing arguments from the innermost environment.
    // On all other architectures caller drops outgoing arguments itself
    // hence the difference.
    pending_deoptimization_env_->DropArguments(instr->ArgumentCount());
    AddDeoptIndexAtCall(deopt_id_after, instr->token_pos());
  } else {
    // Add deoptimization continuation point after the call and before the
    // arguments are removed.
    // In optimized code this descriptor is needed for exception handling.
    AddCurrentDescriptor(RawPcDescriptors::kDeopt,
                         deopt_id_after,
                         instr->token_pos());
  }
}


void CompilerDeoptInfoWithStub::GenerateCode(FlowGraphCompiler* compiler,
                                             intptr_t stub_ix) {
  UNIMPLEMENTED();
}


#define __ assembler()->


void FlowGraphCompiler::GenerateAssertAssignable(TokenPosition token_pos,
                                                 intptr_t deopt_id,
                                                 const AbstractType& dst_type,
                                                 const String& dst_name,
                                                 LocationSummary* locs) {
  SubtypeTestCache& test_cache = SubtypeTestCache::Handle();
  if (!dst_type.IsVoidType() && dst_type.IsInstantiated()) {
    test_cache = SubtypeTestCache::New();
  }

  if (is_optimizing()) {
    __ Push(locs->in(0).reg());
    __ Push(locs->in(1).reg());
  }
  __ PushConstant(dst_type);
  __ PushConstant(dst_name);
  __ AssertAssignable(__ AddConstant(test_cache));
  RecordSafepoint(locs);
  AddCurrentDescriptor(RawPcDescriptors::kOther, deopt_id, token_pos);
  if (is_optimizing()) {
    // Assert assignable keeps the instance on the stack as the result,
    // all other arguments are popped.
    // In optimized code we need to drop it because optimized code
    // expects the result in the register and it is already there
    // because locs()->in(0).reg() == locs()->out(0).reg().
    __ Drop1();
  }
}


void FlowGraphCompiler::EmitInstructionEpilogue(Instruction* instr) {
  if (!is_optimizing()) {
    Definition* defn = instr->AsDefinition();
    if ((defn != NULL) &&
        (defn->tag() != Instruction::kPushArgument) &&
        (defn->tag() != Instruction::kStoreIndexed) &&
        (defn->tag() != Instruction::kStoreStaticField) &&
        (defn->tag() != Instruction::kStoreLocal) &&
        (defn->tag() != Instruction::kStoreInstanceField) &&
        (defn->tag() != Instruction::kDropTemps) &&
        !defn->HasTemp()) {
      __ Drop1();
    }
  }
}


void FlowGraphCompiler::GenerateInlinedGetter(intptr_t offset) {
  __ Move(0, -(1 + kParamEndSlotFromFp));
  __ LoadField(0, 0, offset / kWordSize);
  __ Return(0);
}


void FlowGraphCompiler::GenerateInlinedSetter(intptr_t offset) {
  __ Move(0, -(2 + kParamEndSlotFromFp));
  __ Move(1, -(1 + kParamEndSlotFromFp));
  __ StoreField(0, offset / kWordSize, 1);
  __ LoadConstant(0, Object::Handle());
  __ Return(0);
}


void FlowGraphCompiler::EmitFrameEntry() {
  const Function& function = parsed_function().function();
  const intptr_t num_fixed_params = function.num_fixed_parameters();
  const int num_opt_pos_params = function.NumOptionalPositionalParameters();
  const int num_opt_named_params = function.NumOptionalNamedParameters();
  const int num_params =
      num_fixed_params + num_opt_pos_params + num_opt_named_params;
  const bool has_optional_params = (num_opt_pos_params != 0) ||
      (num_opt_named_params != 0);
  const int num_locals = parsed_function().num_stack_locals();
  const intptr_t context_index =
      -parsed_function().current_context_var()->index() - 1;

  if (CanOptimizeFunction() &&
      function.IsOptimizable() &&
      (!is_optimizing() || may_reoptimize())) {
    __ HotCheck(!is_optimizing(), GetOptimizationThreshold());
  }

  if (has_optional_params) {
    __ EntryOptional(num_fixed_params,
                     num_opt_pos_params,
                     num_opt_named_params);
  } else if (!is_optimizing()) {
    __ Entry(num_fixed_params, num_locals, context_index);
  } else {
    __ EntryOptimized(num_fixed_params,
                      flow_graph_.graph_entry()->spill_slot_count());
  }

  if (num_opt_named_params != 0) {
    LocalScope* scope = parsed_function().node_sequence()->scope();

    // Start by alphabetically sorting the names of the optional parameters.
    LocalVariable** opt_param =
        zone()->Alloc<LocalVariable*>(num_opt_named_params);
    int* opt_param_position = zone()->Alloc<int>(num_opt_named_params);
    for (int pos = num_fixed_params; pos < num_params; pos++) {
      LocalVariable* parameter = scope->VariableAt(pos);
      const String& opt_param_name = parameter->name();
      int i = pos - num_fixed_params;
      while (--i >= 0) {
        LocalVariable* param_i = opt_param[i];
        const intptr_t result = opt_param_name.CompareTo(param_i->name());
        ASSERT(result != 0);
        if (result > 0) break;
        opt_param[i + 1] = opt_param[i];
        opt_param_position[i + 1] = opt_param_position[i];
      }
      opt_param[i + 1] = parameter;
      opt_param_position[i + 1] = pos;
    }

    for (intptr_t i = 0; i < num_opt_named_params; i++) {
      const int param_pos = opt_param_position[i];
      const Instance& value = parsed_function().DefaultParameterValueAt(
          param_pos - num_fixed_params);
      __ LoadConstant(param_pos, opt_param[i]->name());
      __ LoadConstant(param_pos, value);
    }
  } else if (num_opt_pos_params != 0) {
    for (intptr_t i = 0; i < num_opt_pos_params; i++) {
      const Object& value = parsed_function().DefaultParameterValueAt(i);
      __ LoadConstant(num_fixed_params + i, value);
    }
  }


  if (has_optional_params) {
    if (!is_optimizing()) {
      ASSERT(num_locals > 0);  // There is always at least context_var.
      __ Frame(num_locals);  // Reserve space for locals.
    } else if (flow_graph_.graph_entry()->spill_slot_count() >
                   flow_graph_.num_copied_params()) {
      __ Frame(flow_graph_.graph_entry()->spill_slot_count() -
          flow_graph_.num_copied_params());
    }
  }

  if (function.IsClosureFunction()) {
    Register reg = is_optimizing() ? flow_graph_.num_copied_params()
                                   : context_index;
    Register closure_reg = reg;
    LocalScope* scope = parsed_function().node_sequence()->scope();
    LocalVariable* local = scope->VariableAt(0);
    if (local->index() > 0) {
      __ Move(reg, -local->index());
    } else {
      closure_reg = -local->index() - 1;
    }
    __ LoadField(reg, closure_reg, Closure::context_offset() / kWordSize);
  } else if (has_optional_params && !is_optimizing()) {
    __ LoadConstant(context_index,
        Object::Handle(isolate()->object_store()->empty_context()));
  }
}


void FlowGraphCompiler::CompileGraph() {
  InitCompiler();

  if (TryIntrinsify()) {
    // Skip regular code generation.
    return;
  }

  EmitFrameEntry();
  VisitBlocks();
}


#undef __
#define __ compiler_->assembler()->


void ParallelMoveResolver::EmitMove(int index) {
  MoveOperands* move = moves_[index];
  const Location source = move->src();
  const Location destination = move->dest();
  if (source.IsStackSlot() && destination.IsRegister()) {
    // Only allow access to the arguments.
    ASSERT(source.base_reg() == FPREG);
    ASSERT(source.stack_index() < 0);
    __ Move(destination.reg(), -kParamEndSlotFromFp + source.stack_index());
  } else if (source.IsRegister() && destination.IsRegister()) {
    __ Move(destination.reg(), source.reg());
  } else if (source.IsConstant() && destination.IsRegister()) {
    __ LoadConstant(destination.reg(), source.constant());
  } else {
    compiler_->Bailout("Unsupported move");
  }

  move->Eliminate();
}


void ParallelMoveResolver::EmitSwap(int index) {
  MoveOperands* move = moves_[index];
  const Location source = move->src();
  const Location destination = move->dest();
  ASSERT(source.IsRegister() && destination.IsRegister());
  __ Swap(destination.reg(), source.reg());

  // The swap of source and destination has executed a move from source to
  // destination.
  move->Eliminate();

  // Any unperformed (including pending) move with a source of either
  // this move's source or destination needs to have their source
  // changed to reflect the state of affairs after the swap.
  for (int i = 0; i < moves_.length(); ++i) {
    const MoveOperands& other_move = *moves_[i];
    if (other_move.Blocks(source)) {
      moves_[i]->set_src(destination);
    } else if (other_move.Blocks(destination)) {
      moves_[i]->set_src(source);
    }
  }
}


void ParallelMoveResolver::MoveMemoryToMemory(const Address& dst,
                                              const Address& src) {
  UNREACHABLE();
}


void ParallelMoveResolver::StoreObject(const Address& dst, const Object& obj) {
  UNREACHABLE();
}


// Do not call or implement this function. Instead, use the form below that
// uses an offset from the frame pointer instead of an Address.
void ParallelMoveResolver::Exchange(Register reg, const Address& mem) {
  UNREACHABLE();
}


// Do not call or implement this function. Instead, use the form below that
// uses offsets from the frame pointer instead of Addresses.
void ParallelMoveResolver::Exchange(const Address& mem1, const Address& mem2) {
  UNREACHABLE();
}


void ParallelMoveResolver::Exchange(Register reg,
                                    Register base_reg,
                                    intptr_t stack_offset) {
  UNIMPLEMENTED();
}


void ParallelMoveResolver::Exchange(Register base_reg1,
                                    intptr_t stack_offset1,
                                    Register base_reg2,
                                    intptr_t stack_offset2) {
  UNIMPLEMENTED();
}


void ParallelMoveResolver::SpillScratch(Register reg) {
  UNIMPLEMENTED();
}


void ParallelMoveResolver::RestoreScratch(Register reg) {
  UNIMPLEMENTED();
}


void ParallelMoveResolver::SpillFpuScratch(FpuRegister reg) {
  UNIMPLEMENTED();
}


void ParallelMoveResolver::RestoreFpuScratch(FpuRegister reg) {
  UNIMPLEMENTED();
}


#undef __

}  // namespace dart

#endif  // defined TARGET_ARCH_DBC
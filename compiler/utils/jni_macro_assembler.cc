/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "jni_macro_assembler.h"

#include <algorithm>
#include <vector>

#ifdef ART_ENABLE_CODEGEN_arm
#include "arm/jni_macro_assembler_arm_vixl.h"
#endif
#ifdef ART_ENABLE_CODEGEN_arm64
#include "arm64/jni_macro_assembler_arm64.h"
#endif
#ifdef ART_ENABLE_CODEGEN_riscv64
#include "riscv64/jni_macro_assembler_riscv64.h"
#endif
#ifdef ART_ENABLE_CODEGEN_x86
#include "x86/jni_macro_assembler_x86.h"
#endif
#ifdef ART_ENABLE_CODEGEN_x86_64
#include "x86_64/jni_macro_assembler_x86_64.h"
#endif
#include "base/casts.h"
#include "base/globals.h"
#include "base/memory_region.h"
#include "gc_root.h"
#include "jni/jni_env_ext.h"
#include "jni/local_reference_table.h"
#include "stack_reference.h"

namespace art HIDDEN {

using MacroAsm32UniquePtr = std::unique_ptr<JNIMacroAssembler<PointerSize::k32>>;

template <>
MacroAsm32UniquePtr JNIMacroAssembler<PointerSize::k32>::Create(
    ArenaAllocator* allocator,
    InstructionSet instruction_set,
    const InstructionSetFeatures* instruction_set_features) {
  // TODO: Remove the parameter from API (not needed after Mips target was removed).
  UNUSED(instruction_set_features);

  switch (instruction_set) {
#ifdef ART_ENABLE_CODEGEN_arm
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      return MacroAsm32UniquePtr(new (allocator) arm::ArmVIXLJNIMacroAssembler(allocator));
#endif
#ifdef ART_ENABLE_CODEGEN_x86
    case InstructionSet::kX86:
      return MacroAsm32UniquePtr(new (allocator) x86::X86JNIMacroAssembler(allocator));
#endif
    default:
      UNUSED(allocator);
      LOG(FATAL) << "Unknown/unsupported 4B InstructionSet: " << instruction_set;
      UNREACHABLE();
  }
}

using MacroAsm64UniquePtr = std::unique_ptr<JNIMacroAssembler<PointerSize::k64>>;

template <>
MacroAsm64UniquePtr JNIMacroAssembler<PointerSize::k64>::Create(
    ArenaAllocator* allocator,
    InstructionSet instruction_set,
    const InstructionSetFeatures* instruction_set_features) {
  // TODO: Remove the parameter from API (not needed after Mips64 target was removed).
  UNUSED(instruction_set_features);

  switch (instruction_set) {
#ifdef ART_ENABLE_CODEGEN_arm64
    case InstructionSet::kArm64:
      return MacroAsm64UniquePtr(new (allocator) arm64::Arm64JNIMacroAssembler(allocator));
#endif
#ifdef ART_ENABLE_CODEGEN_riscv64
    case InstructionSet::kRiscv64:
      return MacroAsm64UniquePtr(new (allocator) riscv64::Riscv64JNIMacroAssembler(allocator));
#endif
#ifdef ART_ENABLE_CODEGEN_x86_64
    case InstructionSet::kX86_64:
      return MacroAsm64UniquePtr(new (allocator) x86_64::X86_64JNIMacroAssembler(allocator));
#endif
    default:
      UNUSED(allocator);
      LOG(FATAL) << "Unknown/unsupported 8B InstructionSet: " << instruction_set;
      UNREACHABLE();
  }
}

template <PointerSize kPointerSize>
void JNIMacroAssembler<kPointerSize>::LoadGcRootWithoutReadBarrier(ManagedRegister dest,
                                                                   ManagedRegister base,
                                                                   MemberOffset offs) {
  static_assert(sizeof(uint32_t) == sizeof(GcRoot<mirror::Object>));
  Load(dest, base, offs, sizeof(uint32_t));
}

template
void JNIMacroAssembler<PointerSize::k32>::LoadGcRootWithoutReadBarrier(ManagedRegister dest,
                                                                       ManagedRegister base,
                                                                       MemberOffset offs);
template
void JNIMacroAssembler<PointerSize::k64>::LoadGcRootWithoutReadBarrier(ManagedRegister dest,
                                                                       ManagedRegister base,
                                                                       MemberOffset offs);

template <PointerSize kPointerSize>
void JNIMacroAssembler<kPointerSize>::LoadStackReference(ManagedRegister dest, FrameOffset offs) {
  static_assert(sizeof(uint32_t) == sizeof(StackReference<mirror::Object>));
  Load(dest, offs, sizeof(uint32_t));
}

template
void JNIMacroAssembler<PointerSize::k32>::LoadStackReference(ManagedRegister dest,
                                                             FrameOffset offs);
template
void JNIMacroAssembler<PointerSize::k64>::LoadStackReference(ManagedRegister dest,
                                                             FrameOffset offs);

template <PointerSize kPointerSize>
void JNIMacroAssembler<kPointerSize>::PushLocalReferenceFrame(ManagedRegister jni_env_reg,
                                                              ManagedRegister saved_cookie_reg,
                                                              ManagedRegister temp_reg) {
  constexpr size_t kLRTSegmentStateSize = sizeof(jni::LRTSegmentState);
  const MemberOffset jni_env_cookie_offset = JNIEnvExt::LocalRefCookieOffset(kPointerSize);
  const MemberOffset jni_env_segment_state_offset = JNIEnvExt::SegmentStateOffset(kPointerSize);

  // Load the old cookie that we shall need to restore.
  Load(saved_cookie_reg, jni_env_reg, jni_env_cookie_offset, kLRTSegmentStateSize);

  // Set the cookie to the current segment state.
  Load(temp_reg, jni_env_reg, jni_env_segment_state_offset, kLRTSegmentStateSize);
  Store(jni_env_reg, jni_env_cookie_offset, temp_reg, kLRTSegmentStateSize);
}

template
void JNIMacroAssembler<PointerSize::k32>::PushLocalReferenceFrame(ManagedRegister jni_env_reg,
                                                                  ManagedRegister saved_cookie_reg,
                                                                  ManagedRegister temp_reg);
template
void JNIMacroAssembler<PointerSize::k64>::PushLocalReferenceFrame(ManagedRegister jni_env_reg,
                                                                  ManagedRegister saved_cookie_reg,
                                                                  ManagedRegister temp_reg);

template <PointerSize kPointerSize>
void JNIMacroAssembler<kPointerSize>::PopLocalReferenceFrame(ManagedRegister jni_env_reg,
                                                             ManagedRegister saved_cookie_reg,
                                                             ManagedRegister temp_reg) {
  constexpr size_t kLRTSegmentStateSize = sizeof(jni::LRTSegmentState);
  const MemberOffset jni_env_cookie_offset = JNIEnvExt::LocalRefCookieOffset(kPointerSize);
  const MemberOffset jni_env_segment_state_offset = JNIEnvExt::SegmentStateOffset(kPointerSize);

  // Set the current segment state to the current cookie.
  Load(temp_reg, jni_env_reg, jni_env_cookie_offset, kLRTSegmentStateSize);
  Store(jni_env_reg, jni_env_segment_state_offset, temp_reg, kLRTSegmentStateSize);

  // Restore the cookie to the saved value.
  Store(jni_env_reg, jni_env_cookie_offset, saved_cookie_reg, kLRTSegmentStateSize);
}

template
void JNIMacroAssembler<PointerSize::k32>::PopLocalReferenceFrame(ManagedRegister jni_env_reg,
                                                                 ManagedRegister saved_cookie_reg,
                                                                 ManagedRegister temp_reg);
template
void JNIMacroAssembler<PointerSize::k64>::PopLocalReferenceFrame(ManagedRegister jni_env_reg,
                                                                 ManagedRegister saved_cookie_reg,
                                                                 ManagedRegister temp_reg);

}  // namespace art
